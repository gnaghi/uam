
project('uam', ['c', 'cpp'],
	version: '1.1.0',
	default_options: [ 'buildtype=release', 'strip=true', 'b_ndebug=if-release', 'c_std=c99', 'cpp_std=c++17' ],
)

prog_python = import('python3').find_python()
has_mako = run_command(
	prog_python, '-c',
	'''
import mako
v = tuple(int(x) for x in mako.__version__.split("."))
assert v > (0, 8, 0)
	''')
if has_mako.returncode() != 0
	error('Python (3.x) mako module >= 0.8.0 required to build uam.')
endif

compiler_c = meson.get_compiler('c')
compiler_cpp = meson.get_compiler('cpp')

prog_bison = find_program('bison', native: true)
prog_flex = find_program('flex', native: true)

pre_args = [
	'-DUAM_VERSION_MAJOR=@0@'.format(meson.project_version().split('.')[0]),
	'-DUAM_VERSION_MINOR=@0@'.format(meson.project_version().split('.')[1]),
	'-DUAM_VERSION_MICRO=@0@'.format(meson.project_version().split('.')[2]),
	'-DPACKAGE_STRING="@0@ @1@"'.format(meson.project_name(), meson.project_version()),
	'-D_USE_MATH_DEFINES',
]

if host_machine.system() != 'windows'
	pre_args += '-D_GNU_SOURCE'
	foreach f : [ 'posix_memalign' ]
		if compiler_c.has_function(f)
			pre_args += '-DHAVE_@0@'.format(f.to_upper())
		endif
	endforeach
else
	pre_args += [
		'-mno-ms-bitfields',
		'-Werror=format-extra-args',
		'-D__STDC_FORMAT_MACROS',
		'-D__USE_MINGW_ANSI_STDIO=1',
	]
endif

foreach a : [ '-ffunction-sections' ]
	if compiler_c.has_argument(a)
		pre_args += a
	endif
endforeach

foreach a : pre_args
	add_project_arguments(a, language : ['c', 'cpp'])
endforeach

foreach a : [ '-Werror=implicit-function-declaration' ]
	if compiler_c.has_argument(a)
		add_project_arguments(a, language : 'c')
	endif
endforeach

foreach a : [ '-Wno-class-memaccess', '-Wno-non-virtual-dtor' ]
	if compiler_cpp.has_argument(a)
		add_project_arguments(a, language : 'cpp')
	endif
endforeach

foreach a : [ '-Wl,--gc-sections' ]
	if compiler_c.has_link_argument(a)
		add_project_link_arguments(a, language : [ 'c', 'cpp' ])
	endif
endforeach

uam_files = []
uam_incs = []
uam_install_headers = []

uam_incs += include_directories('source')

# ============================================================================
# glslang sources (compiled directly into uam, no external library)
# ============================================================================
glslang_root = get_option('glslang_root')
if glslang_root != ''
	glslang_dir = glslang_root

	uam_incs += include_directories(glslang_dir)
	uam_incs += include_directories(glslang_dir / 'glslang' / 'Include')
	# DirStackFileIncluder.h is in StandAlone/
	uam_incs += include_directories(glslang_dir / 'StandAlone')

	# glslang core: MachineIndependent
	glslang_mi_files = []
	foreach f : [
		'attribute.cpp', 'Constant.cpp', 'glslang_tab.cpp', 'InfoSink.cpp',
		'Initialize.cpp', 'IntermTraverse.cpp', 'Intermediate.cpp',
		'iomapper.cpp', 'limits.cpp', 'linkValidate.cpp', 'parseConst.cpp',
		'ParseContextBase.cpp', 'ParseHelper.cpp', 'PoolAlloc.cpp',
		'propagateNoContraction.cpp', 'reflection.cpp', 'RemoveTree.cpp',
		'Scan.cpp', 'ShaderLang.cpp', 'SpirvIntrinsics.cpp',
		'SymbolTable.cpp', 'Versions.cpp', 'intermOut.cpp',
	]
		glslang_mi_files += glslang_dir / 'glslang' / 'MachineIndependent' / f
	endforeach

	# Preprocessor
	foreach f : ['Pp.cpp', 'PpAtom.cpp', 'PpContext.cpp', 'PpScanner.cpp', 'PpTokens.cpp']
		glslang_mi_files += glslang_dir / 'glslang' / 'MachineIndependent' / 'preprocessor' / f
	endforeach

	# GenericCodeGen
	glslang_mi_files += glslang_dir / 'glslang' / 'GenericCodeGen' / 'CodeGen.cpp'
	glslang_mi_files += glslang_dir / 'glslang' / 'GenericCodeGen' / 'Link.cpp'

	# C Interface
	glslang_mi_files += glslang_dir / 'glslang' / 'CInterface' / 'glslang_c_interface.cpp'

	# ResourceLimits (default resource + C API)
	glslang_mi_files += glslang_dir / 'glslang' / 'ResourceLimits' / 'ResourceLimits.cpp'
	glslang_mi_files += glslang_dir / 'glslang' / 'ResourceLimits' / 'resource_limits_c.cpp'

	# OS-dependent (threading)
	if host_machine.system() == 'windows'
		glslang_mi_files += glslang_dir / 'glslang' / 'OSDependent' / 'Windows' / 'ossource.cpp'
	else
		glslang_mi_files += glslang_dir / 'glslang' / 'OSDependent' / 'Unix' / 'ossource.cpp'
	endif

	# SPIR-V backend
	foreach f : [
		'GlslangToSpv.cpp', 'InReadableOrder.cpp', 'Logger.cpp',
		'SpvBuilder.cpp', 'SpvPostProcess.cpp', 'SpvTools.cpp',
		'disassemble.cpp', 'doc.cpp',
	]
		glslang_mi_files += glslang_dir / 'SPIRV' / f
	endforeach
	glslang_mi_files += glslang_dir / 'SPIRV' / 'CInterface' / 'spirv_c_interface.cpp'

	# glslang-specific compile flags (C++17, no HLSL, no SPIRV-Tools optimizer)
	glslang_cpp_args = ['-DENABLE_OPT=0']
	# Switch (hos) lacks __cxa_thread_atexit for thread_local C++ objects.
	# glslang's PoolAlloc.cpp uses thread_local TPoolAllocator which crashes.
	# DISABLE_THREAD_SUPPORT makes it a plain global (fine: single-threaded).
	if host_machine.system() == 'hos'
		glslang_cpp_args += '-DDISABLE_THREAD_SUPPORT'
	endif
	# Suppress warnings from glslang code we don't control
	foreach a : ['-Wno-unused-parameter', '-Wno-sign-compare', '-Wno-missing-field-initializers',
	             '-Wno-switch', '-Wno-deprecated-copy']
		if compiler_cpp.has_argument(a)
			glslang_cpp_args += a
		endif
	endforeach

	glslang_lib = static_library(
		'glslang_imported',
		glslang_mi_files,
		include_directories: uam_incs,
		cpp_args: glslang_cpp_args,
	)
	glslang_libs = [glslang_lib]
else
	glslang_libs = []
endif

subdir('source')
subdir('mesa-imported')

build_mode = get_option('build_mode')

if build_mode == 'lib' or build_mode == 'both'
	install_headers(uam_install_headers, subdir: 'uam')

	uamlib = static_library(
		'uam',
		uam_files,
		include_directories: uam_incs,
		link_whole: glslang_libs,
		install: true,
	)

	pkgcfg = import('pkgconfig')
	pkgcfg.generate(uamlib)
endif

if build_mode == 'exe' or build_mode == 'both'
	uamexe = executable(
		'uam',
		uam_files + files('source/main.cpp'),
		c_args: ['-DDESKTOP'],
		cpp_args: ['-DDESKTOP'],
		include_directories: uam_incs,
		link_whole: glslang_libs,
		install: true,
	)
endif

# SPIR-V parser unit tests (standalone, no NV50_IR dependency)
test_spirv_parser = executable(
	'test_spirv_parser',
	files('tests/test_spirv_parser.cpp', 'source/spirv_frontend.cpp'),
	cpp_args: ['-DDESKTOP'],
	include_directories: uam_incs,
	install: false,
)
test('spirv_parser', test_spirv_parser)
