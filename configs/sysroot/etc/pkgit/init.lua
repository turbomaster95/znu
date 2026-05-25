local home = os.getenv("HOME")
local prefix = home.."/.local"
install_directories = {
	bin		= prefix.."/bin",
	include	= prefix.."/include",
	lib		= prefix.."/lib",
	src		= prefix.."/src",
	pkgblds	= home.."/.local/share/pkgit",
}

repositories = {
	pkgit = {
		url = "https://git.symlinx.net/pkgit",
	},
	beaker = {
		url = "https://git.symlinx.net/beaker",
		dependencies = {},
	},
}

build_systems = {
	Makefile = {
		build = function()
			os.execute("make")
		end,
	},
	["CMakeLists.txt"] = {
		build = function()
			os.execute("cmake -B build")
			os.execute("cmake --build build")
		end,
	},
}