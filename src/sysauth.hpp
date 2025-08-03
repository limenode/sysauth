#pragma once
#include "config.hpp"
#include "polkit.hpp"

#include <memory>

class sysauth {
	public:
		sysauth(const std::map<std::string, std::map<std::string, std::string>>&);
		~sysauth();

		bool initialize_polkit();
		void cleanup_polkit();

	private:
		std::map<std::string, std::map<std::string, std::string>> config_main;
		std::unique_ptr<polkit_listener> listener_instance;
		PolkitSubject* polkit_subject;
		bool polkit_initialized;

		void load_stylesheets();
};

extern "C" {
	sysauth *sysauth_create(const std::map<std::string, std::map<std::string, std::string>>&);
}