#include <cairo.h>
#include <poppler.h>
#include <math.h>
#include <assert.h>

int main(int argc, char** argv)
{
   assert (argc > 1);
   g_type_init();
   char *filename = "file:///home/beza1e1/dev/pdfpresenter/example.pdf";
   double doc_w, doc_h;
   PopplerDocument *document = poppler_document_new_from_file(filename, NULL, NULL);
   int page_num = 2;
   PopplerPage *page = poppler_document_get_page(document, page_num);
   poppler_page_get_size(page, &doc_w, &doc_h);
   cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)ceil(doc_w), (int)ceil(doc_h));
   cairo_t *cr = cairo_create(surface);
   poppler_page_render(page, cr);

   cairo_surface_destroy(surface);
   return 0;
}
