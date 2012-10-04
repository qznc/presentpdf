#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include <cairo.h>
#include <poppler.h>
#include <clutter/clutter.h>

#if CLUTTER_MAJOR_VERSION != 1 || CLUTTER_MINOR_VERSION < 8
#error Wrong clutter version!
#endif

#define IMAGE_DPI 150

/** presenter stage shows the display for the talker */
static ClutterStage *presenter_stage;
/** show stage is the display for the audience */
static ClutterStage *show_stage;
/** number of slides */
static unsigned slide_count;
/** currently shown slide */
static unsigned current_slide_index;
/** count down timer */
static ClutterText *onscreen_clock;
/** crossfading between show slides */
static ClutterTimeline *crossfading;

static const unsigned SLIDE_X_RESOLUTION = 1024;
static const unsigned SLIDE_Y_RESOLUTION = 768;
static const unsigned CROSSFADE_MSEC = 200;
static const unsigned FPS = 60;

typedef struct slide_info {
	unsigned index;
	cairo_surface_t *src_surface;
	ClutterActor *actor;
	ClutterActor *show_actor;
} slide_info;
/** internal info for each slide */
static slide_info *slide_meta_data;

static gboolean draw_slide(ClutterCairoTexture *canvas, cairo_t *cr, gpointer data)
{
	slide_info *info = (slide_info*) data;
	cairo_surface_t *surface = info->src_surface;
	const int width  = cairo_image_surface_get_width (surface);
	const int height = cairo_image_surface_get_height (surface);

	/* scale for the rendering */
	double scale_x = clutter_actor_get_width(CLUTTER_ACTOR(canvas)) / width;
	double scale_y = clutter_actor_get_height(CLUTTER_ACTOR(canvas)) / height;
	cairo_scale(cr, scale_x, scale_y);

	/* render */
	cairo_set_source_surface (cr, surface, 0, 0);
	cairo_paint (cr);

	return true;
}

static void read_pdf(char *filename)
{
	PopplerDocument *document = poppler_document_new_from_file(filename, NULL, NULL);
	const unsigned count = (unsigned) poppler_document_get_n_pages(document);
	slide_meta_data = malloc(sizeof(slide_info) * count);
	slide_count = count;

	/* render slides */
	for (unsigned i=0; i<count; ++i) {
		slide_info *info = &(slide_meta_data[i]);
		info->src_surface = NULL;
		PopplerPage *page = poppler_document_get_page (document, i);
		double width, height;
		poppler_page_get_size (page, &width, &height);

		/* For correct rendering of PDF, the PDF is first rendered to a
		 * transparent image (all alpha = 0). */
		cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
				IMAGE_DPI*width/72.0,
				IMAGE_DPI*height/72.0);
		cairo_t *cr = cairo_create (surface);
		cairo_scale (cr, IMAGE_DPI/72.0, IMAGE_DPI/72.0);
		cairo_save (cr);
		poppler_page_render (page, cr);
		cairo_restore (cr);

		/* Then the image is painted on top of a white "page". Instead of
		 * creating a second image, painting it white, then painting the
		 * PDF image over it we can use the CAIRO_OPERATOR_DEST_OVER
		 * operator to achieve the same effect with the one image. */
		cairo_set_operator (cr, CAIRO_OPERATOR_DEST_OVER);
		cairo_set_source_rgb (cr, 1, 1, 1);
		cairo_paint (cr);

		cairo_status_t status = cairo_status(cr);
		if (status)
			printf("%s\n", cairo_status_to_string (status));

		cairo_destroy (cr);
		slide_meta_data[i].src_surface = surface;
	}
}

static void ensure_slide_actor(unsigned i)
{
	slide_info *info = &(slide_meta_data[i]);
	if (info->actor != NULL) return;

	ClutterCairoTexture *show_canvas = (ClutterCairoTexture*)
		clutter_cairo_texture_new(SLIDE_X_RESOLUTION, SLIDE_Y_RESOLUTION);
	ClutterCairoTexture *presenter_canvas = (ClutterCairoTexture*)
		clutter_cairo_texture_new(400, 300);
	info->index = i;

	info->show_actor = CLUTTER_ACTOR(show_canvas);
	//printf("added slide %d to show stage\n", i);
	info->actor = CLUTTER_ACTOR(presenter_canvas);
	//printf("added slide %d to presenter stage\n", i);

	g_signal_connect (show_canvas, "draw", G_CALLBACK (draw_slide), info);
	g_signal_connect (presenter_canvas, "draw", G_CALLBACK (draw_slide), info);

	if (i == current_slide_index) {
		clutter_actor_show(CLUTTER_ACTOR(show_canvas));
		clutter_actor_show(CLUTTER_ACTOR(presenter_canvas));
	} else {
		clutter_actor_hide(CLUTTER_ACTOR(show_canvas));
		clutter_actor_hide(CLUTTER_ACTOR(presenter_canvas));
	}

	clutter_container_add_actor (CLUTTER_CONTAINER(show_stage), CLUTTER_ACTOR(show_canvas));
	clutter_container_add_actor (CLUTTER_CONTAINER(presenter_stage), CLUTTER_ACTOR(presenter_canvas));

	/* bind the size of the canvas to that of the stage */
	clutter_actor_add_constraint(CLUTTER_ACTOR(show_canvas),
			clutter_bind_constraint_new(CLUTTER_ACTOR(show_stage), CLUTTER_BIND_SIZE, 0));

	/* trigger draw */
	clutter_cairo_texture_invalidate(CLUTTER_CAIRO_TEXTURE(show_canvas));
	clutter_cairo_texture_invalidate(CLUTTER_CAIRO_TEXTURE(presenter_canvas));
}

static void init_slide_actors(void)
{
	const unsigned count = slide_count;

	for (unsigned i=0; i<count; ++i) {
		slide_info *info = &(slide_meta_data[i]);
		info->actor = NULL;
		info->show_actor = NULL;
	}
}

static void place_slides(void)
{
	const float rotation = -25;

	/* hide all */
	for (unsigned i=0; i<slide_count; ++i) {
		ClutterActor *a = slide_meta_data[i].actor;
		if (NULL != a)
			clutter_actor_hide(a);
	}
	const float stage_width = clutter_actor_get_width(CLUTTER_ACTOR(presenter_stage));
	ensure_slide_actor(current_slide_index);

	/* show current slide */
	ClutterActor *current = slide_meta_data[current_slide_index].actor;
	const float slide_width = clutter_actor_get_width(CLUTTER_ACTOR(current));
	const float x = (stage_width - slide_width) / 2.0;
	clutter_actor_set_position(current, x, 0);
	clutter_actor_set_rotation(current, CLUTTER_Y_AXIS, 0, 0, 0, 0);
	clutter_actor_raise_top(current);
	clutter_actor_show(current);

	/* show current show slide */
	ClutterActor *current_show = slide_meta_data[current_slide_index].show_actor;
	clutter_actor_show(current_show);
	clutter_actor_set_opacity(current_show, 0);
	clutter_actor_raise_top(current_show);
	clutter_timeline_rewind(crossfading);
	clutter_timeline_start(crossfading);

	/* show previous slide */
	if (current_slide_index > 0) {
		ensure_slide_actor(current_slide_index-1);
		ClutterActor *previous = slide_meta_data[current_slide_index-1].actor;
		clutter_actor_set_position(previous, x-slide_width-1, 0);
		clutter_actor_set_rotation(previous, CLUTTER_Y_AXIS, rotation, slide_width, 0, 0);
		clutter_actor_show(previous);
	}

	/* show next slide */
	if (current_slide_index+1 < slide_count) {
		ensure_slide_actor(current_slide_index+1);
		ClutterActor *next = slide_meta_data[current_slide_index+1].actor;
		clutter_actor_set_position(next, x+slide_width+1, 0);
		clutter_actor_set_rotation(next, CLUTTER_Y_AXIS, -rotation, 0, 0, 0);
		clutter_actor_show(next);
	}
}

static void next_slide(void)
{
	if (current_slide_index+1 == slide_count)
		return;
	current_slide_index += 1;
	place_slides();
}

static void previous_slide(void)
{
	if (current_slide_index == 0)
		return;
	current_slide_index -= 1;
	place_slides();
}

static void to_slide(unsigned index)
{
	if (index >= slide_count)
		index = slide_count-1;
	current_slide_index = index;
	place_slides();
}

static void handle_key_input(ClutterCairoTexture *canvas, ClutterEvent *ev)
{
	(void)canvas;
	ClutterKeyEvent *event = (ClutterKeyEvent*) ev;
	switch (event->keyval) {
		case 65363: /* RIGHT */
		case 65364: /* DOWN */
		case ' ':
			next_slide();
			break;
		case 65361: /* LEFT */
		case 65362: /* UP */
		case 65288: /* BACKSPACE */
			previous_slide();
			break;
		case 65307: /* ESCAPE */
		case 'q':
		case 'Q': /* exit */
			clutter_main_quit();
			break;
		case 65360: /* Pos1 */
			to_slide(0);
			break;
		case 65367: /* End */
			to_slide(slide_count-1);
			break;
		case 'f':
			clutter_stage_set_fullscreen(show_stage, !clutter_stage_get_fullscreen(show_stage));
			place_slides();
			break;
		default: /* ignore */
			printf("key press %d (%d %d)\n", event->keyval, event->hardware_keycode, event->unicode_value);
			break;
	}
}

static void handle_button_input(ClutterCairoTexture *canvas, ClutterEvent *ev)
{
	(void)canvas;
	ClutterButtonEvent *event = (ClutterButtonEvent*) ev;
	switch (event->button) {
		case 1: /* left click */
			next_slide();
			break;
		case 3: /* right click */
			previous_slide();
			break;
		default: /* ignore */
			printf("button press %d\n", event->button);
			break;
	}
}

static int min(int x, int y) {
	if (x < y) return x;
	return y;
}

static void update_crossfade(void)
{
	const unsigned OPACITY_CHANGE = 1 + (FPS * 255 / CROSSFADE_MSEC);
	for (unsigned i=0; i<slide_count; ++i) {
		ClutterActor *s = slide_meta_data[i].show_actor;
		if (NULL == s) continue;
		guint8 opacity = clutter_actor_get_opacity(s);
		if (i == current_slide_index) {
			guint8 diff = 255-opacity;
			opacity += min(OPACITY_CHANGE, diff);
		}
		clutter_actor_set_opacity(s, opacity);
	}
}

static void create_show_stage(void)
{
	show_stage = CLUTTER_STAGE(clutter_stage_new());

	ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };
	clutter_stage_set_color (CLUTTER_STAGE (show_stage), &stage_color);
	clutter_actor_set_size(CLUTTER_ACTOR(show_stage), 640, 480);
	clutter_stage_set_title(show_stage, "Presentation Window");

	clutter_actor_show (CLUTTER_ACTOR(show_stage));

	g_signal_connect (show_stage, "key-press-event",
			G_CALLBACK (handle_key_input),
			NULL);
	g_signal_connect (show_stage, "button-press-event",
			G_CALLBACK (handle_button_input),
			NULL);

	crossfading = clutter_timeline_new(3*CROSSFADE_MSEC);
	clutter_timeline_set_delay(crossfading, 1000 / FPS);
	g_signal_connect(crossfading, "new-frame", G_CALLBACK(update_crossfade), NULL);
}

static void create_presenter_stage(void)
{
	presenter_stage = CLUTTER_STAGE(clutter_stage_new());

	ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };
	clutter_stage_set_color (CLUTTER_STAGE (presenter_stage), &stage_color);
	clutter_actor_set_size(CLUTTER_ACTOR(presenter_stage), 800, 600);
	clutter_stage_set_title(presenter_stage, "PDF Presenter");

	clutter_actor_show (CLUTTER_ACTOR(presenter_stage));

	g_signal_connect (presenter_stage, "key-press-event",
			G_CALLBACK (handle_key_input),
			NULL);
}

static void update_time(void)
{
	time_t t = time(NULL);
	struct tm *tmp = localtime(&t);
	char time_string[20];
	strftime(time_string, 20, "%H:%M:%S", tmp);
	clutter_text_set_text(CLUTTER_TEXT(onscreen_clock), time_string);
}

static void create_onscreen_clock(void)
{
	ClutterColor text_color = { 0xff, 0xff, 0xcc, 0xff };
	onscreen_clock = CLUTTER_TEXT(clutter_text_new());
	clutter_text_set_font_name(CLUTTER_TEXT(onscreen_clock), "Mono 24px");
	clutter_text_set_color(CLUTTER_TEXT(onscreen_clock), &text_color);
	clutter_actor_set_position (CLUTTER_ACTOR(onscreen_clock), 5, 300);
	clutter_container_add_actor(CLUTTER_CONTAINER(presenter_stage), CLUTTER_ACTOR(onscreen_clock));
	clutter_actor_show (CLUTTER_ACTOR(onscreen_clock));
	update_time();
}

int main(int argc, char** argv)
{
	if (argc == 1) {
		printf("Usage: %s [filename]\n", argv[0]);
		return 0;
	}
	assert (argc == 2);
	char filename[1000] = "file://"; // TODO length safe?
	realpath(argv[1], filename+7);

	g_type_init();
	if (clutter_init(&argc, &argv) != CLUTTER_INIT_SUCCESS)
		return 1;

	assert (clutter_feature_available(CLUTTER_FEATURE_STAGE_MULTIPLE));

	create_show_stage();
	create_presenter_stage();
	read_pdf(filename);
	init_slide_actors();
	create_onscreen_clock();
	place_slides();

	ClutterTimeline *timeline = clutter_timeline_new(1000);
	g_signal_connect(timeline, "new-frame", G_CALLBACK(update_time), NULL);
	clutter_timeline_set_loop(timeline, TRUE);
	clutter_timeline_start(timeline);

	printf("GO!\n");
	clutter_main();

	return 0;
}
