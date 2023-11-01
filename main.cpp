// Common C Libraries
#include <cstdlib>
#include <cstdio>

// GTK
#include <gtk/gtk.h>

// Singleton
#include <fcntl.h>
#include <unistd.h>

// GDBus
#include <gio/gio.h>

// Authentication format string
const char *authAddressFMT = "https://developer.api.autodesk.com/authentication/v2/authorize?response_type=code"
                             "&client_id=%s"
                             "&redirect_uri=apsshelldemo://oauth"
                             "&scope=data:read%%20data:create%%20data:write";

// Some consts for our well-known D-Bus service
const char *dbus_object_path = "/das/apsshelldemo/object";
const char *dbus_interface_name = "das.apsshelldemo";
const char *dbus_method_name = "doOAuth";
const char *dbus_well_known_name = "das.apshelldemo.dbusserver";

char empty[] = "";

// D-Bus ids.
static guint registration_id;
static guint owner_id;

// Accessing GTK items in the dbus callback
typedef struct {
    GApplication *app;
    GtkLabel *label;
} UserData;

// We'll generate GDBusNodeInfo from XML and store it.
static GDBusNodeInfo *introspection_data = NULL;

// This XML defines our method interface for clients calling
static const gchar introspection_xml[] =
        "<node>"
        "<interface name='das.apsshelldemo'>"
        "<method name='doOAuth'>"
        "<arg name='message' type='s' direction='in'/>"
        "</method>"
        "</interface>"
        "</node>";

// This is the method for handling dbus method call.
static void handle_method_call(GDBusConnection *connection,
                               const gchar *sender,
                               const gchar *object_path,
                               const gchar *interface_name,
                               const gchar *method_name,
                               GVariant *parameters,
                               GDBusMethodInvocation *invocation,
                               gpointer user_data) {
    // Check which method is calling. We only have one method here.
    if (g_strcmp0(method_name, dbus_method_name) == 0) {
        const gchar *message;
        g_variant_get(parameters, "(&s)", &message);
        if (g_strcmp0(message, "Return Unregistered") == 0 || g_strcmp0(message, "Return Registered") == 0 ||
            g_strcmp0(message, "Return Raw") == 0) {
            // Error, do nothing
        } else {
            // Unwrap our user data and set the label.
            GtkLabel *label = ((UserData *) user_data)->label;
            gtk_label_set_text(label, message);
        }
    }

    // Return NULL to allow client continue.
    g_dbus_method_invocation_return_value(invocation, NULL);
}

// We only want to handle method calling here
static const GDBusInterfaceVTable interface_vtable =
        {
                handle_method_call,
                NULL,
                NULL,
                {0}
        };

// callback function which is called when button is clicked
static void on_button_clicked(GtkButton *btn, gpointer data) {
    // Let's generate the uri.
    char buffer[1024];
    auto env = std::getenv("APS_CLIENT_ID");
    if (NULL == env) {
        env = empty;
    }
    std::snprintf(buffer, 1024, authAddressFMT, env);

    // Launch our uri in a window.
    gtk_show_uri_on_window(NULL, buffer, GDK_CURRENT_TIME, NULL);
}

// Free user data
static void user_data_free(gpointer data) {
    free(data);
}

// Register D-Bus object after acquring the bus.
static void on_bus_acquired(GDBusConnection *connection,
                            const gchar *name,
                            gpointer user_data) {
    registration_id = g_dbus_connection_register_object(connection,
                                                        dbus_object_path,
                                                        introspection_data->interfaces[0],
                                                        &interface_vtable,
                                                        user_data,  /* user_data */
                                                        user_data_free,  /* user_data_free_func */
                                                        NULL); /* GError** */
}

static void on_name_acquired(GDBusConnection *connection,
                             const gchar *name,
                             gpointer user_data) {
}


// We would like to close our application when name is lost.
static void on_name_lost(GDBusConnection *connection,
                         const gchar *name,
                         gpointer user_data) {
    UserData *data = (UserData *) user_data;
    // exit
    g_application_quit(data->app);
}

// callback function which is called when application is first started
static void on_app_activate(GApplication *app, gpointer data) {
    // create a new application window for the application
    // GtkApplication is sub-class of GApplication
    // downcast GApplication* to GtkApplication* with GTK_APPLICATION() macro
    GtkWidget *window = gtk_application_window_new(GTK_APPLICATION(app));
    GtkWidget *vbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(vbox, 5);
    gtk_widget_set_margin_end(vbox, 5);
    gtk_widget_set_margin_top(vbox, 5);
    gtk_widget_set_margin_bottom(vbox, 5);



    // Check if APS_CLIENT_ID exists
    // We can add it through user profile or ide environment.
    // If we want to add it system-wide, we can add it to /etc/environment
    char buffer[256];
    auto env = getenv("APS_CLIENT_ID");
    if (NULL == env) {
        std::sprintf(buffer, "You need to set APS_CLIENT_ID in your environment variables.");
    } else {
        std::sprintf(buffer, "CLIENT ID:%s", env);
    }



    // Create UI
    GtkWidget *label = gtk_label_new(buffer);
    GtkWidget *btn = gtk_button_new_with_label("Do Auth!");
    // connect the event-handler for "clicked" signal of button
    g_signal_connect(btn, "clicked", G_CALLBACK(on_button_clicked), NULL);
    // add the button to the window
    gtk_container_add(GTK_CONTAINER(vbox), btn);
    gtk_container_add(GTK_CONTAINER(vbox), label);
    gtk_container_add(GTK_CONTAINER(window), vbox);


    // Let's setup our D-Bus through GDBus
    // Using XML to create our introspection is easier.
    introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);

    // We'll wrap our GTK widgets in user data and use them in the D-Bus callback.
    UserData *user_data = new UserData();
    user_data->app = app;
    user_data->label = (GtkLabel*)label;


    // Let's connect to D-Bus!
    // We are using the bus of login session.
    owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                              dbus_well_known_name,
                              G_BUS_NAME_OWNER_FLAGS_NONE,
                              on_bus_acquired,
                              on_name_acquired,
                              on_name_lost,
                              user_data,

                              user_data_free);
    // display the window
    gtk_widget_show_all(GTK_WIDGET(window));
}


int main(int argc, char *argv[]) {
    // We are using file lock here, alternatives could be socket or a semaphore
    int fd = open("/tmp/apsshelldemo.pid", O_CREAT | O_RDWR, 0666);
    if (lockf(fd, F_TLOCK, 0) == 0) {

        // create new GtkApplication with an unique application ID
        GtkApplication *app = gtk_application_new(
                "DAS.APSShellDemo",
                G_APPLICATION_FLAGS_NONE
        );

        // connect the event-handler for "activate" signal of GApplication
        // G_CALLBACK() macro is used to cast the callback function pointer
        // to generic void pointer
        g_signal_connect(app, "activate", G_CALLBACK(on_app_activate), NULL);
        // start the application, terminate by closing the window
        // GtkApplication* is upcast to GApplication* with G_APPLICATION() macro

        int status = g_application_run(G_APPLICATION(app), 0, 0);
        // deallocate the application object
        g_object_unref(app);

        g_dbus_connection_unregister_object(g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL), registration_id);
        g_bus_unown_name(owner_id);

        // Unlock our pid file.
        lockf(fd, F_ULOCK, 0);
        close(fd);
        return status;
    } else {
        // Check if there is an input for sending through dbus
        if (argc < 2) {
            return 0;
        }

        // We already have a window running
        // Send a signal through D-Bus
        // Connect to the D-Bus login session bus

        GError *error = NULL;
        GDBusConnection *connection;
        connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
        if (error) {
            g_print("Failed to connect to the D-Bus: %s\n", error->message);
            g_error_free(error);
            return 1;
        }

        // Create a new proxy object for the server's D-Bus interface
        GDBusProxy *proxy;
        proxy = g_dbus_proxy_new_sync(connection,
                                      G_DBUS_PROXY_FLAGS_NONE,
                                      NULL,
                                      dbus_well_known_name,
                                      dbus_object_path,
                                      dbus_interface_name,
                                      NULL,
                                      &error);

        if (error) {
            g_print("Failed to create D-Bus proxy: %s\n", error->message);
            g_error_free(error);
            return 1;
        }


        // Call the oauth method on the server
        g_dbus_proxy_call_sync(proxy,
                               dbus_method_name,
                               g_variant_new("(s)", argv[1]),
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               NULL,
                               &error);

        if (error) {
            g_print("Failed to call D-Bus method: %s\n", error->message);
            g_error_free(error);
            return 1;
        }

        // Clean up and exit
        g_object_unref(proxy);
        g_object_unref(connection);
        return 0;
    }
}