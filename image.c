// The current file is an example of the implementation of the library, using gtk to view the image on a new window 

#include <gtk/gtk.h>
#undef FALSE // Prevent redifinition
#undef TRUE  // Prevent redifinition
#define _NO_LIBRARY_ // Use directly the library for this example
#include "./include/image_io.h"

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    (void) widget;
    Image* image = (Image*) user_data;

	unsigned int image_size = image -> width * image -> height;
	unsigned char* image_data = (unsigned char*) calloc(image_size * 4, sizeof(unsigned char));
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, image -> width);

	int is_little_endian = 1;
	if (*((char*) &is_little_endian)) is_little_endian = 1;
	else is_little_endian = 0;

	for (unsigned int i = 0; i < image_size; i++) {
        image_data[i * 4 + 2] = is_little_endian ? image -> decoded_data[i * 3 + 0] : image -> decoded_data[i * 3 + 2]; // Red
        image_data[i * 4 + 1] = image -> decoded_data[i * 3 + 1]; // Green
        image_data[i * 4 + 0] = is_little_endian ? image -> decoded_data[i * 3 + 2] : image -> decoded_data[i * 3 + 0]; // Blue
    }

    cairo_surface_t *image_surface = cairo_image_surface_create_for_data(
        image_data,                     // Image data (RGB format)
        CAIRO_FORMAT_RGB24,              // Format
        image -> width,                    // Image width
        image -> height,                   // Image height
        stride                 // Stride (width * 4 for RGB24)
    );

    // Set the surface as the source
    cairo_set_source_surface(cr, image_surface, 25, 25);

    // Paint the image on the drawing area
    cairo_paint(cr);

    // Clean up
    cairo_surface_destroy(image_surface);

	free(image_data);

	return 0; // Event handled, no need to propagate further
}

void draw_image(char* filename, Image* image) {
    int count = 1;
    char** data = (char**) calloc(1, sizeof(char*));
    // Select a name
    data[0] = filename;
    gtk_init(&count, &data);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 200, 200);

    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), drawing_area);

    // Connect the draw event to the callback function
    g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(on_draw_event), (void*) image);

    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);

    gtk_main();

    return;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Invalid command syntax: image 'src_img.jpg'\n");
        return -1;
    }

    bool status;
    char* file_name = argv[1];
    Image image = decode_image(file_name);

    if (image.error) {
        image.error = CLAMP(image.error, 0, sizeof(err_codes) / sizeof(err_codes[0]));
        error_print("terminate the program with the error code: %s\n", err_codes[image.error]);
        return (image.error);
    }

    draw_image(file_name, &image);

    if ((status = create_ppm_image(image, "./out/new_image.ppm"))) {
        error_print("terminate the program with the error code: %s\n", err_codes[status]);
        return status;
    }
	
	free(image.decoded_data);

    return 0;
}
