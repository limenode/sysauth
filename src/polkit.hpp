#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
#include <polkitagent/polkitagent.h>
#include <polkit/polkit.h>
#include <gtkmm/window.h>
#include <gtkmm/revealer.h>
#include <gtkmm/box.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/button.h>
#include <glibmm.h>
#include <gio/gio.h>
#include <memory>

class polkit_auth_dialog : public Gtk::Window {
	public:
		polkit_auth_dialog(const std::string& action_id, const std::string& message, const std::string& icon);

		Gtk::Revealer revealer_layout;
		Gtk::Entry entry_password;

		bool run();

	private:
		bool result_accepted;
		std::shared_ptr<Glib::MainLoop> main_loop;

		Gtk::Box box_layout;
		Gtk::Image image_program;
		Gtk::Label label_program;
		Gtk::Label label_description;
		Gtk::Box box_buttons;
		Gtk::Button button_cancel;
		Gtk::Button button_ok;

		void on_button_clicked(const bool& accepted);
};

class polkit_listener {
	public:
		polkit_listener();
		virtual ~polkit_listener();
		
		PolkitAgentListener* get_listener() {
			return listener;
		}
		
		void keep_alive() { 
			if (listener) g_object_ref(listener);
		}
		
		void release() {
			if (listener) g_object_unref(listener);
		}

		void handle_authentication(const std::string& action_id,
								const std::string& message,
								const std::string& icon_name,
								const std::string& cookie,
								GList* identities,
								GAsyncReadyCallback callback,
								gpointer user_data);

	private:
		PolkitAgentListener* listener;
		bool is_registered;
};

#define CUSTOM_TYPE_POLKIT_AGENT_LISTENER (custom_polkit_agent_listener_get_type())
G_DECLARE_FINAL_TYPE(CustomPolkitAgentListener, custom_polkit_agent_listener, CUSTOM, POLKIT_AGENT_LISTENER, PolkitAgentListener)

struct _CustomPolkitAgentListenerClass {
	PolkitAgentListenerClass parent_class;
};

struct _CustomPolkitAgentListener {
	PolkitAgentListener parent_instance;
	polkit_listener* cpp_instance;
};