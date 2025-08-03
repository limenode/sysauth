#include "sysauth.hpp"

#include <gtkmm/cssprovider.h>
#include <filesystem>

sysauth::sysauth(const std::map<std::string, std::map<std::string, std::string>>& cfg) 
	: config_main(cfg) {

	polkit_initialized = initialize_polkit();

	// TODO: Make this not crap out if ran through sysshell
	if (!polkit_initialized)
		exit(1);

	load_stylesheets();
}

sysauth::~sysauth() {
	cleanup_polkit();
}

void sysauth::load_stylesheets() {
	const std::string& style_path = "/usr/share/sys64/auth/style.css";
	const std::string& style_path_usr = std::string(getenv("HOME")) + "/.config/sys64/auth/style.css";

	// Load base style
	if (std::filesystem::exists(style_path)) {
		auto css = Gtk::CssProvider::create();
		css->load_from_path(style_path);
		Gtk::StyleContext::add_provider_for_display(
			Gdk::Display::get_default(),
			css,
			GTK_STYLE_PROVIDER_PRIORITY_USER
		);
	}
	// Load user style
	if (std::filesystem::exists(style_path_usr)) {
		auto css = Gtk::CssProvider::create();
		css->load_from_path(style_path_usr);
		Gtk::StyleContext::add_provider_for_display(
			Gdk::Display::get_default(),
			css,
			GTK_STYLE_PROVIDER_PRIORITY_USER
		);
	}
}

bool sysauth::initialize_polkit() {
	GError* error = nullptr;
	polkit_subject = polkit_unix_session_new_for_process_sync(getpid(), nullptr, &error);
	
	if (error) {
		std::fprintf(stderr, "Failed to create polkit subject: %s\n", error->message);
		g_error_free(error);
		return false;
	}
	
	listener_instance = std::make_unique<polkit_listener>();
	listener_instance->keep_alive();
	
	PolkitAgentListener* agent_listener = listener_instance->get_listener();
	if (!polkit_agent_listener_register(agent_listener,
										POLKIT_AGENT_REGISTER_FLAGS_NONE,
										polkit_subject,
										nullptr,
										nullptr,
										&error)) {
		std::fprintf(stderr, "Failed to register polkit agent: %s\n", error->message);
		g_error_free(error);
		g_object_unref(polkit_subject);
		polkit_subject = nullptr;
		listener_instance.reset();
		return false;
	}

	return true;
}

void sysauth::cleanup_polkit() {
	if (!polkit_initialized)
		return;

	if (listener_instance) {
		PolkitAgentListener* agent_listener = listener_instance->get_listener();
		if (agent_listener)
			polkit_agent_listener_unregister(agent_listener);

		listener_instance->release();
		listener_instance.reset();
	}
	
	if (polkit_subject) {
		g_object_unref(polkit_subject);
		polkit_subject = nullptr;
	}

	polkit_initialized = false;
}

extern "C" {
	sysauth* sysauth_create(const std::map<std::string, std::map<std::string, std::string>>& cfg) {
		return new sysauth(cfg);
	}
}
