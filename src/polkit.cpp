#include "polkit.hpp"
#include <gtk4-layer-shell.h>

polkit_auth_dialog::polkit_auth_dialog(const std::string& action_id, const std::string& message, const std::string& icon) : Gtk::Window() {
	// TODO: Ideally move this somewhere else and reuse window related stuff for the keyring (To be added later)
	// TODO: Add an error label & error handling

	gtk_layer_init_for_window(gobj());
	gtk_layer_set_keyboard_mode(gobj(), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
	gtk_layer_set_namespace(gobj(), "sysauth");
	gtk_layer_set_layer(gobj(), GTK_LAYER_SHELL_LAYER_OVERLAY);

	gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
	gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);
	gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_TOP, true);
	gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, true);

	set_name("sysauth");
	set_title("Authenticate");
	add_css_class("primary_window");

	set_child(revealer_layout);

	revealer_layout.set_child(box_layout);
	revealer_layout.set_transition_duration(1000);
	revealer_layout.set_transition_type(Gtk::RevealerTransitionType::SLIDE_UP);

	box_layout.get_style_context()->add_class("box_layout");
	box_layout.get_style_context()->add_class("card");
	box_layout.property_orientation().set_value(Gtk::Orientation::VERTICAL);
	box_layout.set_halign(Gtk::Align::CENTER);
	box_layout.set_valign(Gtk::Align::CENTER);
	box_layout.set_size_request(340, 400);
	
	box_layout.append(image_program);
	box_layout.append(label_program);
	box_layout.append(label_description);
	box_layout.append(entry_password);
	box_layout.append(box_buttons);
	box_buttons.append(button_cancel);
	box_buttons.append(button_ok);

	image_program.set_from_icon_name(icon);
	image_program.set_pixel_size(96);
	image_program.set_vexpand();

	label_program.set_text(action_id);
	label_program.get_style_context()->add_class("title-1");
	label_program.set_margin(10);
	label_program.set_ellipsize(Pango::EllipsizeMode::END);
	label_program.set_max_width_chars(0);

	label_description.set_text(message);
	label_description.set_justify(Gtk::Justification::CENTER);
	label_description.set_ellipsize(Pango::EllipsizeMode::END);
	label_description.set_max_width_chars(0);
	label_description.set_wrap(true);
	label_description.set_lines(3);

	entry_password.set_placeholder_text("Password");
	entry_password.set_margin(10);
	entry_password.set_margin_bottom(5);
	entry_password.set_vexpand();
	entry_password.set_valign(Gtk::Align::END);
	entry_password.set_icon_from_icon_name("lock-symbolic");
	entry_password.set_input_hints(Gtk::InputHints::PRIVATE);
	entry_password.set_input_purpose(Gtk::InputPurpose::PASSWORD);
	entry_password.set_visibility(false);
	entry_password.grab_focus();

	box_buttons.set_margin(5);
	box_buttons.set_homogeneous(true);
	box_buttons.set_orientation(Gtk::Orientation::HORIZONTAL);

	button_cancel.set_label("Cancel");
	button_cancel.set_margin(5);

	button_ok.set_label("Ok");
	button_ok.set_margin(5);
	button_ok.get_style_context()->add_class("suggested-action");

	button_ok.signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &polkit_auth_dialog::on_button_clicked), true));
	button_cancel.signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &polkit_auth_dialog::on_button_clicked), false));
	entry_password.signal_activate().connect(sigc::bind(sigc::mem_fun(*this, &polkit_auth_dialog::on_button_clicked), true));
}

bool polkit_auth_dialog::run() {
	main_loop = Glib::MainLoop::create();
	show();
	revealer_layout.set_reveal_child(true);
	main_loop->run();
	return result_accepted;
}

void polkit_auth_dialog::on_button_clicked(const bool& accepted) {
	result_accepted = accepted;

	if (accepted)
		revealer_layout.set_transition_duration(500);
	revealer_layout.set_reveal_child(false);

	auto timeout_connection = Glib::signal_timeout().connect([&]() {
		main_loop->quit();
		return false;
	}, accepted ? 500 : 250);
}
G_DEFINE_TYPE(CustomPolkitAgentListener, custom_polkit_agent_listener, POLKIT_AGENT_TYPE_LISTENER)

polkit_listener::polkit_listener() : is_registered(false) {
	listener = POLKIT_AGENT_LISTENER(g_object_new(CUSTOM_TYPE_POLKIT_AGENT_LISTENER, nullptr));
	CUSTOM_POLKIT_AGENT_LISTENER(listener)->cpp_instance = this;
	g_object_ref_sink(listener);
}

polkit_listener::~polkit_listener() {
	if (listener) {
		CUSTOM_POLKIT_AGENT_LISTENER(listener)->cpp_instance = nullptr;
		g_object_unref(listener);
		listener = nullptr;
	}
}

static void session_completed_cb(PolkitAgentSession* session,
								gboolean gained_authorization,
								gpointer user_data) {
	struct session_data {
		GAsyncReadyCallback callback;
		gpointer user_data;
		std::string password;
		polkit_auth_dialog* dialog;
		PolkitAgentListener* listener;
		PolkitAgentSession* session;
	};
	
	session_data* data = static_cast<session_data*>(user_data);
	
	if (data->callback) {
		GTask* task = g_task_new(G_OBJECT(data->listener), nullptr, data->callback, data->user_data);
		if (!gained_authorization) {
			g_task_return_error(task, g_error_new(POLKIT_ERROR,
												  POLKIT_ERROR_FAILED,
												  "Authentication failed"));
		} else {
			g_task_return_boolean(task, TRUE);
		}
		g_object_unref(task);
	}
	
	g_object_unref(data->session);
	delete data;
}

static void session_request_cb(PolkitAgentSession* session,
							   const gchar* request,
							   gboolean echo_on,
							   gpointer user_data) {
	struct session_data {
		GAsyncReadyCallback callback;
		gpointer user_data;
		std::string password;
		polkit_auth_dialog* dialog;
		PolkitAgentListener* listener;
		PolkitAgentSession* session;
	};
	
	session_data* data = static_cast<session_data*>(user_data);
	polkit_agent_session_response(session, data->password.c_str());
}

static void session_show_error_cb(PolkitAgentSession* session,
								  const gchar* text,
								  gpointer user_data) {
	struct session_data {
		GAsyncReadyCallback callback;
		gpointer user_data;
		std::string password;
		polkit_auth_dialog* dialog;
		PolkitAgentListener* listener;
		PolkitAgentSession* session;
	};
	
	//session_data* data = static_cast<session_data*>(user_data);
	std::fprintf(stderr, text ? text : "Authentication error");
}

static void initiate_authentication_cb(PolkitAgentListener* listener,
									   const gchar* action_id,
									   const gchar* message,
									   const gchar* icon_name,
									   PolkitDetails* details,
									   const gchar* cookie,
									   GList* identities,
									   GCancellable* cancellable,
									   GAsyncReadyCallback callback,
									   gpointer user_data) {
	
	CustomPolkitAgentListener* custom_listener = CUSTOM_POLKIT_AGENT_LISTENER(listener);
	polkit_listener* cpp_instance = custom_listener->cpp_instance;
	
	if (!cpp_instance) {
		if (callback) {
			GTask* task = g_task_new(G_OBJECT(listener), nullptr, callback, user_data);
			g_task_return_error(task, g_error_new(POLKIT_ERROR,
												  POLKIT_ERROR_FAILED,
												  "Listener instance destroyed"));
			g_object_unref(task);
		}
		return;
	}
	
	std::string action_str = action_id ? action_id : "";
	std::string message_str = message ? message : "";
	std::string icon_str = icon_name ? icon_name : "";
	std::string cookie_str = cookie ? cookie : "";

	if (action_str == "org.freedesktop.policykit.exec") {
		std::string result = message_str.substr(message_str.find_last_of('/') + 1);
		result = result.substr(0, result.find('\''));
		action_str = result;
		icon_str = result;
	}
	else if (action_str.find('.')) {
		action_str = action_str.substr(action_str.find_last_of('.') + 1);
	}
	
	cpp_instance->handle_authentication(action_str, message_str, icon_str, cookie_str, identities, callback, user_data);
}

static gboolean initiate_authentication_finish_cb(PolkitAgentListener* listener,
												  GAsyncResult* res,
												  GError** error) {
	if (G_IS_TASK(res)) {
		return g_task_propagate_boolean(G_TASK(res), error);
	}
	
	if (error) {
		*error = g_error_new(POLKIT_ERROR, POLKIT_ERROR_FAILED, "Invalid async result");
	}
	return FALSE;
}

void polkit_listener::handle_authentication(const std::string& action_id,
											const std::string& message,
											const std::string& icon_name,
											const std::string& cookie,
											GList* identities,
											GAsyncReadyCallback callback,
											gpointer user_data) {
	
	if (!identities) {
		if (callback) {
			GTask* task = g_task_new(G_OBJECT(listener), nullptr, callback, user_data);
			g_task_return_error(task, g_error_new(POLKIT_ERROR,
												  POLKIT_ERROR_CANCELLED,
												  "No identities provided"));
			g_object_unref(task);
		}
		return;
	}
	
	PolkitIdentity* identity = static_cast<PolkitIdentity*>(identities->data);
	
	if (!POLKIT_IS_UNIX_USER(identity)) {
		if (callback) {
			GTask* task = g_task_new(G_OBJECT(listener), nullptr, callback, user_data);
			g_task_return_error(task, g_error_new(POLKIT_ERROR,
												  POLKIT_ERROR_CANCELLED,
												  "Unsupported identity type"));
			g_object_unref(task);
		}
		return;
	}
	
	polkit_auth_dialog dialog(action_id, message, icon_name);
	
	while (true) {
		int response = dialog.run();
		
		if (response != 1) {
			if (callback) {
				GTask* task = g_task_new(G_OBJECT(listener), nullptr, callback, user_data);
				g_task_return_error(task, g_error_new(POLKIT_ERROR,
													  POLKIT_ERROR_CANCELLED,
													  "Authentication cancelled"));
				g_object_unref(task);
			}
			break;
		}
		
		std::string password = dialog.entry_password.get_text();
		
		if (password.empty()) {
			std::fprintf(stderr, "Password cannot be empty\n");
			continue;
		}
		
		PolkitAgentSession* session = polkit_agent_session_new(identity, cookie.c_str());
		
		struct session_data {
			GAsyncReadyCallback callback;
			gpointer user_data;
			std::string password;
			polkit_auth_dialog* dialog;
			PolkitAgentListener* listener;
			PolkitAgentSession* session;
		};
		
		session_data* data = new session_data{callback, user_data, password, &dialog, listener, session};
		
		g_object_ref(session);
		
		g_signal_connect(session, "completed", G_CALLBACK(session_completed_cb), data);
		g_signal_connect(session, "request", G_CALLBACK(session_request_cb), data);
		g_signal_connect(session, "show-error", G_CALLBACK(session_show_error_cb), data);
		
		polkit_agent_session_initiate(session);
		break;
	}
}

static void custom_polkit_agent_listener_class_init(CustomPolkitAgentListenerClass* klass) {
	PolkitAgentListenerClass* listener_class = POLKIT_AGENT_LISTENER_CLASS(klass);
	listener_class->initiate_authentication = initiate_authentication_cb;
	listener_class->initiate_authentication_finish = initiate_authentication_finish_cb;
}

static void custom_polkit_agent_listener_init(CustomPolkitAgentListener* listener) {
	listener->cpp_instance = nullptr;
}