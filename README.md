# sysauth
sysauth is a simple authentication popup<br>

![preview](https://github.com/System64fumo/sysauth/blob/main/preview.gif "preview")

[![Packaging status](https://repology.org/badge/vertical-allrepos/sysauth.svg)](https://repology.org/project/sysauth/versions)


> [!NOTE]
> This is still W.I.P<br>

### Features
* polkit integration
* keyring integration (coming soon)

# Configuration
sysauth can be configured in 2 ways<br>
1: By changing config.hpp and recompiling (Suckless style)<br>
2: Using a config file (~/.config/sys64/auth/config.conf)<br>
3: Using launch arguments<br>
```
arguments:
  -v	Prints version info
```
