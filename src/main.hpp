#pragma once
#include "config_parser.hpp"

class sysauth;

typedef sysauth* (*sysauth_create_func)(const std::map<std::string, std::map<std::string, std::string>>&);
typedef void (*sysauth_show_windows_func)(sysauth*);

sysauth_create_func sysauth_create_ptr;
sysauth_show_windows_func sysauth_show_windows_ptr;
