#include <cairo.h>
#include <poppler.h>
#include <clutter/clutter.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>

/** Load slides from pdf */
void load_slides(char *filename, int *page_count, cairo_surface_t ***pages)
{
   PopplerDocument *document = poppler_document_new_from_file(filename, NULL, NULL);
   const int pc = poppler_document_get_n_pages(document);
   cairo_surface_t **ps = (cairo_surface_t**) malloc(sizeof(void*) * pc);
   assert (ps != NULL);
   *pages = ps;
   *page_count = pc;

   /* initialize surfaces */
   for (int i=0; i<pc; ++i) {
      PopplerPage *page = poppler_document_get_page(document, i);
      double doc_w, doc_h;
      poppler_page_get_size(page, &doc_w, &doc_h);
      cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)ceil(doc_w), (int)ceil(doc_h));
      cairo_t *cr = cairo_create(surface);
      poppler_page_render(page, cr);
      ps[i] = surface;
   }
}

int main(int argc, char** argv)
{
   assert (argc == 2);
   g_type_init();
   char *filename = "file:///home/beza1e1/dev/pdfpresenter/example.pdf";

   cairo_surface_t **pages;
   int page_count;
   load_slides(filename, &page_count, &pages);

   if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
       return 1;

   ClutterActor    *stage = clutter_stage_get_default ();
   ClutterColor     stage_color = { 0x0, 0x0, 0x0, 0xff };
   clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

   /* Create a timeline to manage animation */
   ClutterTimeline *timeline = clutter_timeline_new (6000);
   clutter_timeline_set_loop (timeline, TRUE);

   clutter_actor_show (stage);

   clutter_timeline_start (timeline);

   g_signal_connect (stage, "key-press-event",
         G_CALLBACK (clutter_main_quit),
         NULL);

   clutter_main();

   return 0;
}
