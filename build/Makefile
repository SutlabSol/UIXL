# XUI Launcher build (MSYS2 MINGW64 native, Linux and macOS best-effort)
#
# XUI Launcher is a trimmed fork of Theseus (UIX Desktop) by TeamUIX,
# GPL-3.0-or-later. See ../LICENSE.
#
# Windows (MSYS2 MINGW64 shell):
#   pacman -S make pkgconf mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 \
#             mingw-w64-x86_64-SDL2_mixer mingw-w64-x86_64-vulkan-loader
#   ./fetch-deps.sh          (clones + builds bgfx/bx/bimg once)
#   make -j8                 (outputs ../out/XUI Launcher.exe + runtime files)
#
# make DEBUG=1 for a debug build.

SRC      := ../src
OUT      := ../out
OBJDIR   := obj
TP       := ../third-party

UNAME_S  := $(shell uname -s)

CXX      ?= g++
ifeq ($(CXX),c++)
  CXX := g++
endif

COMMON_CFLAGS = \
	-std=c++17 \
	$(if $(DEBUG),-g -O0,-O2 -DNDEBUG -ffunction-sections -fdata-sections) \
	-MMD -MP \
	-D_DESKTOP \
	-DTHESEUS_DESKTOP \
	-DUIX_DESKTOP=1 \
	-DXUI_LAUNCHER=1 \
	-D_WINDOWS=1 \
	-D_THESEUS_STD_H=1 \
	-fpermissive \
	-fno-strict-aliasing \
	-I$(SRC)/app \
	-I$(SRC)/desktop \
	-I$(SRC)/desktop/imgui \
	-I$(SRC)/shared \
	-I$(SRC)/engine \
	-I$(SRC)/render \
	-I$(TP)/bgfx/include \
	-I$(TP)/bx/include \
	-I$(TP)/bimg/include \
	-DTHESEUS_USE_BGFX=1 \
	-Wno-write-strings \
	-Wno-deprecated-declarations \
	-Wno-reorder \
	-Wno-unused-value \
	-Wno-switch \
	-Wno-pointer-to-int-cast \
	-Wno-int-to-pointer-cast \
	-Wno-tautological-compare \
	-Wno-parentheses \
	-Wno-narrowing \
	-Wno-dangling-else \
	-Wno-format

ifneq (,$(findstring MINGW,$(UNAME_S))$(findstring MSYS,$(UNAME_S)))
  PLATFORM   := win64
  TARGET     := $(OUT)/XUI Launcher.exe
  TARGET_MK  := $(OUT)/XUI\ Launcher.exe
  BGFX_LIB   := $(TP)/bgfx/.build/win64_mingw-gcc/bin
  CFLAGS     := $(COMMON_CFLAGS) -DSDL_MAIN_HANDLED \
                $(shell pkg-config --cflags sdl2 SDL2_mixer) \
                -I$(TP)/bx/include/compat/mingw
  LDFLAGS    := -static-libgcc -static-libstdc++ \
                $(if $(DEBUG),,-s -Wl,--gc-sections) \
                $(if $(DEBUG)$(CONSOLE),,-Wl,--subsystem,windows) -Wl,-emainCRTStartup \
                $(BGFX_LIB)/libbgfxRelease.a $(BGFX_LIB)/libbimgRelease.a $(BGFX_LIB)/libbxRelease.a \
                -lmingw32 $(shell pkg-config --libs sdl2 SDL2_mixer) \
                -lgdi32 -limm32 -lole32 -loleaut32 -lsetupapi -lversion -lwinmm -luuid \
                -lws2_32 -ldbghelp -lavrt -ldwmapi -lcomdlg32 -lpsapi -lshell32
  RES_OBJ    := $(if $(wildcard icon.ico),$(OBJDIR)/xui_launcher_res.o,)
else ifeq ($(UNAME_S),Linux)
  PLATFORM   := linux
  TARGET     := $(OUT)/xui-launcher
  TARGET_MK  := $(TARGET)
  BGFX_LIB   := $(TP)/bgfx/.build/linux64_gcc/bin
  CFLAGS     := $(COMMON_CFLAGS) $(shell sdl2-config --cflags) -I$(TP)/bx/include/compat/linux
  LDFLAGS    := $(BGFX_LIB)/libbgfxRelease.a $(BGFX_LIB)/libbimgRelease.a $(BGFX_LIB)/libbxRelease.a \
                $(shell sdl2-config --libs) -lSDL2_mixer -lvulkan -lX11 -ldl -lpthread -lm
  RES_OBJ    :=
else
  PLATFORM   := mac
  CXX        := clang++
  TARGET     := $(OUT)/xui-launcher
  TARGET_MK  := $(TARGET)
  BGFX_LIB   := $(TP)/bgfx/.build/osx-arm64/bin
  CFLAGS     := $(COMMON_CFLAGS) $(shell sdl2-config --cflags) -I$(TP)/bx/include/compat/osx
  LDFLAGS    := $(BGFX_LIB)/libbgfxRelease.a $(BGFX_LIB)/libbimgRelease.a $(BGFX_LIB)/libbxRelease.a \
                $(shell sdl2-config --libs) -lSDL2_mixer \
                -framework Cocoa -framework Metal -framework MetalKit -framework QuartzCore \
                -framework IOKit -framework CoreFoundation
  RES_OBJ    :=
endif

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
SRCS_ENGINE = \
	$(SRC)/shared/xap_vm.cpp \
	$(SRC)/engine/xap_compile.cpp \
	$(SRC)/render/node_system.cpp \
	$(SRC)/shared/util.cpp \
	$(SRC)/engine/array.cpp \
	$(SRC)/shared/string_util.cpp \
	$(SRC)/engine/math_node.cpp \
	$(SRC)/shared/date_node.cpp

SRCS_FILE = \
	$(SRC)/desktop/xip.cpp \
	$(SRC)/shared/file_util.cpp \
	$(SRC)/desktop/files.cpp \
	$(SRC)/shared/activefile.cpp \
	$(SRC)/shared/settingsfile.cpp

SRCS_RENDER = \
	$(SRC)/shared/screen.cpp \
	$(SRC)/desktop/image.cpp \
	$(SRC)/desktop/texture.cpp \
	$(SRC)/render/tmap_system.cpp \
	$(SRC)/desktop/materials_desktop.cpp \
	$(SRC)/desktop/mesh.cpp \
	$(SRC)/render/shape_render.cpp \
	$(SRC)/desktop/text.cpp \
	$(SRC)/render/starfield.cpp \
	$(SRC)/engine/dotfield_node.cpp

SRCS_UI = \
	$(SRC)/render/scene_groups.cpp \
	$(SRC)/render/hud_node.cpp \
	$(SRC)/render/camera.cpp \
	$(SRC)/render/animation_nodes.cpp \
	$(SRC)/shared/overlay_alert.cpp \
	$(SRC)/shared/recovery_node.cpp \
	$(SRC)/engine/settings_nodes.cpp

SRCS_SYSTEM = \
	$(SRC)/desktop/dashapp.cpp \
	$(SRC)/desktop/dashinit.cpp \
	$(SRC)/desktop/config.cpp \
	$(SRC)/desktop/joystick.cpp \
	$(SRC)/desktop/keyboard.cpp \
	$(SRC)/desktop/locale.cpp \
	$(SRC)/shared/timezone.cpp \
	$(SRC)/desktop/debug.cpp \
	$(SRC)/desktop/memutil.cpp \
	$(SRC)/render/autosmooth.cpp

SRCS_PLATFORM = \
	$(SRC)/desktop/sdl_main.cpp \
	$(SRC)/desktop/launch.cpp \
	$(SRC)/desktop/app_paths.cpp \
	$(SRC)/desktop/osk.cpp \
	$(SRC)/desktop/path_template.cpp \
	$(SRC)/xbox/network.cpp \
	$(SRC)/desktop/desktop_stubs.cpp \
	$(SRC)/desktop/desktop_nodes.cpp \
	$(SRC)/desktop/audio_sdl.cpp \
	$(SRC)/desktop/boot_anim.cpp \
	$(SRC)/desktop/removed_features.cpp

SRCS_APP = $(wildcard $(SRC)/app/*.cpp)

SRCS_IMGUI = \
	$(SRC)/desktop/imgui/imgui.cpp \
	$(SRC)/desktop/imgui/imgui_draw.cpp \
	$(SRC)/desktop/imgui/imgui_tables.cpp \
	$(SRC)/desktop/imgui/imgui_widgets.cpp \
	$(SRC)/desktop/imgui/imgui_impl_sdl2.cpp \
	$(SRC)/desktop/imgui_impl_bgfx.cpp

ALL_SRCS = $(SRCS_ENGINE) $(SRCS_FILE) $(SRCS_RENDER) $(SRCS_UI) $(SRCS_SYSTEM) \
           $(SRCS_PLATFORM) $(SRCS_APP) $(SRCS_IMGUI)

OBJS = $(patsubst $(SRC)/%.cpp,$(OBJDIR)/%.o,$(ALL_SRCS))
DEPS = $(OBJS:.o=.d)

.PHONY: all clean data run

all: $(TARGET_MK) data
	@echo "=== XUI Launcher built: $(TARGET) ==="

$(TARGET_MK): $(OBJS) $(RES_OBJ)
	@mkdir -p $(OUT)
	$(CXX) -o "$(TARGET)" $(OBJS) $(RES_OBJ) $(LDFLAGS)
ifeq ($(PLATFORM),win64)
	@for d in SDL2.dll SDL2_mixer.dll libwinpthread-1.dll; do \
		f=$$(command -v $$d 2>/dev/null || echo /mingw64/bin/$$d); \
		[ -f "$$f" ] && cp -u "$$f" $(OUT)/ || true; \
	done
	@# SDL2 / SDL2_mixer transitive DLLs (codec libs, SDL3 if sdl2-compat).
	@for dll in $$(ldd "$(TARGET)" 2>/dev/null | awk '/\/mingw64\/bin\// {print $$3}'); do \
		cp -u "$$dll" $(OUT)/ ; \
	done; true
endif

$(OBJDIR)/%.o: $(SRC)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/xui_launcher_res.o: xui_launcher.rc icon.ico
	@mkdir -p $(OBJDIR)
	windres $< -O coff -o $@

# Runtime payload next to the binary. The app seeds the per-user data
# directory (%APPDATA%/XUI Launcher) from here on first run.
data:
	@mkdir -p $(OUT)
	@for D in Configs Data Library; do \
		rsync -a --delete ../$$D/ $(OUT)/$$D/ 2>/dev/null || \
		( rm -rf $(OUT)/$$D && mkdir -p $(OUT)/$$D && cp -R ../$$D/. $(OUT)/$$D/ ); \
	done

run: all
	cd $(OUT) && "./$(notdir $(TARGET))"

clean:
	rm -rf $(OBJDIR) "$(TARGET)"

-include $(DEPS)
