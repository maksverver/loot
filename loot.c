#include <assert.h>
#include <ctype.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define APP_TITLE "Loot"

enum BoxStatus {
  BOX_CLOSED,
  BOX_OPENED,
  BOX_ERROR };

struct BoxConfig {
  const char *name;
  const char *path;
  enum BoxStatus status;
};

static GtkApplication *application;
static const gchar *config_dir;
static GFileMonitor *config_dir_monitor;

static GdkPixbuf *icon_box_opened;
static GdkPixbuf *icon_box_closed;
static GdkPixbuf *icon_box_error;
static GdkPixbuf *icon_reload;
static GdkPixbuf *icon_quit;

static GArray *boxes;

static GtkStatusIcon *status_icon;

pid_t run_command(const char *path, const char *arg, int pipe_fds[2]) {
  pid_t pid = fork();
  assert(pid != -1);
  if (pid == 0) {
    // Child process.
    if (pipe_fds != NULL) {
      close(pipe_fds[0]);
      dup2(pipe_fds[1], 1);
    }
    execl(path, path, arg, (char*)NULL);
    perror("execl");
    exit(1);
  }
  return pid;
}

gboolean wait_for_command(pid_t pid) {
  int status = -1;
  if (waitpid(pid, &status, 0) != pid) {
    perror("waitpid");
    return FALSE;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void box_refresh_status(struct BoxConfig *box) {
  box->status = BOX_ERROR;
  int pipe_fds[2];
  int res = pipe(pipe_fds);
  assert(res == 0);
  pid_t pid = run_command(box->path, "status", pipe_fds);
  close(pipe_fds[1]);
  // We assume the pipe has a large enough buffer that we can wait for the
  // child process to finish writing all its output without blocking.
  if (wait_for_command(pid)) {
    // Read the data written to the pipe
    char buf[100];
    ssize_t nread = read(pipe_fds[0], buf, sizeof(buf) - 1);
    if (nread < 0) {
      perror("read");
    } else {
      buf[nread] = '\0';
      // Terminate at the earliest space character. (Typically newline.)
      char *end = buf;
      while (*end && !isspace(*end)) ++end;
      *end = '\0';
      if (strcmp(buf, "opened") == 0) {
        box->status = BOX_OPENED;
      } else if (strcmp(buf, "closed") == 0) {
        box->status = BOX_CLOSED;
      }
    }
  }
  close(pipe_fds[0]);
}

static gboolean box_open(struct BoxConfig *box) {
  pid_t pid = run_command(box->path, "open", NULL);
  return wait_for_command(pid);
}

static gboolean box_close(struct BoxConfig *box) {
  pid_t pid = run_command(box->path, "close", NULL);
  return wait_for_command(pid);
}

static int box_compare(const void *a, const void *b) {
  const struct BoxConfig *box1 = a;
  const struct BoxConfig *box2 = b;
  return strcmp(box1->name, box2->name);
}

static GdkPixbuf *get_icon_for_status(enum BoxStatus status) {
  switch (status) {
  case BOX_CLOSED:
    return icon_box_closed;
  case BOX_OPENED:
    return icon_box_opened;
  case BOX_ERROR:
  default:
    return icon_box_error;
  }
}
static enum BoxStatus get_combined_status() {
  int num_open = 0;
  int num_error = 0;
  for (int i = 0; i < boxes->len; ++i) {
    struct BoxConfig *box = &g_array_index(boxes, struct BoxConfig, i);
    if (box->status == BOX_OPENED) ++num_open;
    if (box->status == BOX_ERROR) ++num_error;
  }
  return (num_error > 0) ? BOX_ERROR : (num_open > 0) ? BOX_OPENED : BOX_CLOSED;
}

static void show_formatted_error_message(const char *message) {
  GtkWidget *dialog = gtk_message_dialog_new(
      /*parent=*/ NULL, /*flags=*/0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
      "%s", message);
  gtk_window_set_title(GTK_WINDOW(dialog), APP_TITLE " - An error occurred!");
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

static void show_error(const GError *error) {
  show_formatted_error_message(error->message);
}

__attribute__((format(printf, 1, 2)))
static void show_error_message(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  char *buf = g_strdup_vprintf(format, ap);
  va_end(ap);
  show_formatted_error_message(buf);
  g_free(buf);
}

static GdkPixbuf *load_icon(
    const char *data_begin, const char *data_end, GError **error) {
  assert(data_end > data_begin);
  GInputStream *stream = g_memory_input_stream_new_from_data(
    data_begin, data_end - data_begin, NULL);
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, error);
  g_object_unref(stream);
  return pixbuf;
}

static void load_icons(GError **error) {
#define LOAD_ICON(name) \
    extern const char _binary_icons_##name##_png_start[]; \
    extern const char _binary_icons_##name##_png_end[]; \
    icon_##name = load_icon( \
        _binary_icons_##name##_png_start, \
        _binary_icons_##name##_png_end, \
        error); \
    if (*error != NULL) return;
  LOAD_ICON(box_closed);
  LOAD_ICON(box_opened);
  LOAD_ICON(box_error);
  LOAD_ICON(reload);
  LOAD_ICON(quit);
#undef LOAD_ICON
}

static void clear_box(gpointer obj) {
  struct BoxConfig *box = obj;
  g_free((gpointer)box->name);
  box->name = NULL;
  g_free((gpointer)box->path);
  box->path = NULL;
  box->status = BOX_ERROR;
}

static GArray *new_box_array() {
  GArray *array = g_array_new(
      /*zero_terminated=*/ FALSE, /*clear=*/ TRUE, sizeof(struct BoxConfig));
  g_array_set_clear_func(array, clear_box);
  return array;
}

static const char *make_config_dir() {
  const char *config_home = getenv("XDG_CONFIG_HOME");
  if (config_home == NULL || *config_home == '\0') {
    // XDG_CONFIG_HOME is not set. Use "$HOME/.config/loot" instead.
    const char *home = getenv("HOME");
    if (home == NULL || *home == '\0') {
      home = "/";
    }
    return g_build_filename(home, ".config", "loot", NULL);
  } else {
    // "$XDG_CONFIG_HOME/loot"
    return g_build_filename(config_home, "loot", NULL);
  }
}

static GArray *load_boxes(GError **error) {
  GDir *dir = g_dir_open(config_dir, 0, error);
  if (*error != NULL) {
    return NULL;
  }

  // Find executables in the config dir, and add them to the array of boxes.
  GArray *boxes = new_box_array();
  for (const char *name; (name = g_dir_read_name(dir)) != NULL; ) {
    // Skip empty/hidden file (as indicated by leading dot).
    if (!*name || *name == '.') continue;
    char *path = g_build_filename(config_dir, name, NULL);
    GStatBuf attr;
    if (g_stat(path, &attr) != 0 || !S_ISREG(attr.st_mode) || !(attr.st_mode & S_IXUSR)) {
      g_free(path);
    }
    struct BoxConfig config = {
      .name = g_strdup(name),
      .path = path,
      .status = BOX_ERROR };
    g_array_append_val(boxes, config);
  }
  g_dir_close(dir);
  g_array_sort(boxes, box_compare);
  return boxes;
}

static void reload_status_icon() {
  assert(status_icon != NULL);
  gtk_status_icon_set_from_pixbuf(status_icon,
      get_icon_for_status(get_combined_status()));
}

static void reload_boxes() {
  assert(boxes != NULL);
  GError *error = NULL;
  GArray *new_boxes = load_boxes(&error);
  if (error != NULL) {
    assert(new_boxes == NULL);
    show_error(error);
    g_error_free(error);
  } else {
    assert(new_boxes != NULL);
    g_array_free(boxes, TRUE);
    boxes = new_boxes;
    for (int i = 0; i < boxes->len; ++i) {
      box_refresh_status(&g_array_index(boxes, struct BoxConfig, i));
    }
  }
  reload_status_icon();
}

static void activate_menu_item_box(GtkMenuItem *menu_item, gpointer user_data) {
  struct BoxConfig *box = user_data;
  if (box->status == BOX_CLOSED) {
    if (box_open(box)) {
      box->status = BOX_OPENED;
    } else {
      box->status = BOX_ERROR;
      show_error_message("Failed to open box %s!\n", box->name);
    }
  } else if (box->status == BOX_OPENED) {
    if (box_close(box)) {
      box->status = BOX_CLOSED;
    } else {
      box->status = BOX_ERROR;
      show_error_message("Failed to close box %s!\n", box->name);
    }
  } else {
    box_refresh_status(box);
  }
  reload_status_icon();
}

static void activate_menu_item_reload(GtkMenuItem *menu_item, gpointer user_data) {
  reload_boxes();
}

static void activate_menu_item_quit(GtkMenuItem *menu_item, gpointer user_data) {
  g_application_release(G_APPLICATION(application));
}

static GtkWidget *create_menu_item(GtkWidget *icon, GtkWidget *label) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *menu_item = gtk_menu_item_new();
  // TODO: fix icon size and label/icon alignment
  gtk_label_set_xalign(GTK_LABEL(label), 0.0);
  gtk_container_add(GTK_CONTAINER(box), icon);
  gtk_box_pack_end(GTK_BOX(box), label, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(menu_item), box);
  return menu_item;
}

static GtkWidget *create_menu_item_quit() {
  GtkWidget *icon = gtk_image_new_from_pixbuf(icon_quit);
  GtkWidget *label = gtk_label_new("Quit");
  GtkWidget *item = create_menu_item(icon, label);
  g_signal_connect(
      G_OBJECT(item), "activate", G_CALLBACK(activate_menu_item_quit), NULL);
  return item;
}

static GtkWidget *create_menu_item_reload() {
  GtkWidget *icon = gtk_image_new_from_pixbuf(icon_reload);
  GtkWidget *label = gtk_label_new("Reload");
  GtkWidget *item = create_menu_item(icon, label);
  g_signal_connect(
      G_OBJECT(item), "activate", G_CALLBACK(activate_menu_item_reload), NULL);
  return item;
}

static GtkWidget *create_menu_item_box(struct BoxConfig *box) {
  GtkWidget *icon = gtk_image_new_from_pixbuf(get_icon_for_status(box->status));
  GtkWidget *label = gtk_label_new(box->name);
  GtkWidget *item = create_menu_item(icon, label);
  g_signal_connect(
      G_OBJECT(item), "activate", G_CALLBACK(activate_menu_item_box), box);
  return item;
}

static GtkWidget *create_menu() {
  GtkWidget *menu = gtk_menu_new();
  for (int i = 0; i < boxes->len; ++i) {
    struct BoxConfig *box = &g_array_index(boxes, struct BoxConfig, i);
    gtk_menu_shell_append(
        GTK_MENU_SHELL(menu), create_menu_item_box(box));
  }
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), create_menu_item_reload());
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), create_menu_item_quit());
  return menu;
}

static void activate_status_icon(
    GtkStatusIcon *status_icon, gpointer user_data) {
  GtkWidget *menu = create_menu();
  gtk_widget_show_all(menu);
  gtk_menu_popup_at_pointer(GTK_MENU(menu), /*trigger_event=*/NULL);
}

static void popup_menu_status_icon(
    GtkStatusIcon *status_icon, guint button, guint activate_time,
    gpointer user_data) {
  GtkWidget *menu = create_menu();
  gtk_widget_show_all(menu);
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, button, activate_time);
}

void config_dir_changed(GFileMonitor *monitor, GFile *file, GFile *other_file,
    GFileMonitorEvent event_type, gpointer user_data) {
  reload_boxes();
}

static gboolean setup_config_dir(GError **error) {
  gboolean success = FALSE;
  GFile *config_dir_file = g_file_new_for_path(config_dir);
  if (g_file_query_file_type(config_dir_file, G_FILE_QUERY_INFO_NONE, NULL)
      == G_FILE_TYPE_DIRECTORY) {
    success = TRUE;
  } else {
    // Path does not exist (or is not a directory). Try to create it.
    success = g_file_make_directory_with_parents(config_dir_file, NULL, error);
  }

  assert(config_dir_monitor == NULL);
  config_dir_monitor =
      g_file_monitor_directory(config_dir_file, G_FILE_MONITOR_NONE, NULL, NULL);
  if (config_dir_monitor == NULL) {
    // We don't consider this to be a fatal error, because the user can always
    // refresh manually (and box configs don't change that often, anyway).
    g_warning("Failed to monitor config directory (%s).\n", config_dir);
  } else {
    g_signal_connect(
        config_dir_monitor, "changed", G_CALLBACK(config_dir_changed), NULL);
  }
  g_object_unref(config_dir_file);
  return success;
}

static void activate_application(GtkApplication *app, gpointer user_data) {
  if (application != NULL) {
    // activate has been called before. Don't reinitialize anything.
    return;
  }

  application = app;
  config_dir = make_config_dir();

  GError *error = NULL;
  setup_config_dir(&error);
  if (error != NULL) {
    show_error(error);
    g_error_free(error);
    return;
  }

  load_icons(&error);
  if (error != NULL) {
    show_error(error);
    g_error_free(error);
    return;
  }

  status_icon = gtk_status_icon_new_from_pixbuf(icon_box_error);
  if (status_icon == NULL) {
    show_error_message("Failed to created status icon!");
    return;
  }
  gtk_status_icon_set_title(status_icon, APP_TITLE);
  gtk_status_icon_set_tooltip_text(status_icon, "Click here to list available boxes.");
  g_signal_connect(
      status_icon, "activate", G_CALLBACK(activate_status_icon), NULL);
  g_signal_connect(
      status_icon, "popup-menu", G_CALLBACK(popup_menu_status_icon), NULL);

  assert(boxes == NULL);
  boxes = new_box_array();
  reload_boxes();

  // Explicitly hold the application since we didn't create a main window, but
  // we did pop up a status icon, and we want to continue running until the user
  // clicks on it.
  g_application_hold(G_APPLICATION(application));
}

int main (int argc, char **argv) {
  GtkApplication *app = gtk_application_new(
      "ch.verver.maks.loot", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(
      app, "activate", G_CALLBACK(activate_application), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  if (boxes) {
    g_array_free(boxes, TRUE);
  }
  if (icon_box_closed) {
    g_object_unref(icon_box_closed);
  }
  if (icon_box_opened) {
    g_object_unref(icon_box_opened);
  }
  if (icon_box_error) {
    g_object_unref(icon_box_error);
  }
  if (icon_quit) {
    g_object_unref(icon_quit);
  }
  if (icon_reload) {
    g_object_unref(icon_reload);
  }
  if (config_dir) {
    g_free((gpointer)config_dir);
  }
  if (config_dir_monitor) {
    g_object_unref(config_dir_monitor);
  }
  return status;
}
