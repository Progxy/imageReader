// The current file is an example of the implementation of the library, using gtk to print the image on a new window 

#include <gtk/gtk.h>
#undef FALSE // Prevent redifinition
#undef TRUE
#include "./include/image_io.h"

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    (void) widget;
    Image* image = (Image*) user_data;
    unsigned int offset_x = 25;
    unsigned int offset_y = 25;
        
    for (unsigned int i = 0; i < image -> height; ++i) {
        for (unsigned int j = 0; j < image -> width; ++j) {
            unsigned int pos = i * image -> width * (image -> components) + j * (image -> components);
            // Set the color (RGBA) for the pixel
            cairo_set_source_rgba(cr, (image -> decoded_data)[pos] / 255.0, (image -> decoded_data)[pos + 1] / 255.0, (image -> decoded_data)[pos + 2] / 255.0, image -> components == 4 ? (image -> decoded_data)[pos + 3] / 255.0 : 1.0);
            // Draw a filled rectangle (pixel) at position (x, y) with a width and height of 1
            cairo_rectangle(cr, j + offset_x, i + offset_y, 1.0, 1.0);
            cairo_fill(cr);
        }
    }

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
        error_print("Invalid command syntax: image 'src_img.jpg'\n");
        return -1;
    }

    char* file_name = argv[1];
    FileData* image_file = (FileData*) calloc(1, sizeof(FileData));
    bool status;
    
    if ((status = read_image_file(image_file, file_name))) {
        error_print("terminate the program with the error code: %s\n", err_codes[status]);
        return status;
    }

    Image image = decode_image(image_file);

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

    return 0;
}
