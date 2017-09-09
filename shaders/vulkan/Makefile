GLSLC = glslc
GLSLFLAGS = -std=460core --target-env=vulkan

TARGETS = $(patsubst source/%.vert,%_vert.spv,$(wildcard source/*.vert)) \
          $(patsubst source/%.frag,%_frag.spv,$(wildcard source/*.frag))

all: ${TARGETS}

%_vert.spv: source/%.vert
	${GLSLC} ${GLSLFLAGS} $< -o $@

%_frag.spv: source/%.frag
	${GLSLC} ${GLSLFLAGS} $< -o $@

clean:
	rm *.spv
