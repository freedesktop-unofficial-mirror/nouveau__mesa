"""gallium

Frontend-tool for Gallium3D architecture.

"""

# 
# Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
# All Rights Reserved.
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sub license, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
# 
# The above copyright notice and this permission notice (including the
# next paragraph) shall be included in all copies or substantial portions
# of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
# IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
# ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
# 


import os.path

import SCons.Action
import SCons.Builder


def quietCommandLines(env):
	# Quiet command lines
	# See also http://www.scons.org/wiki/HidingCommandLinesInOutput
	env['CCCOMSTR'] = "Compiling $SOURCE ..."
	env['CXXCOMSTR'] = "Compiling $SOURCE ..."
	env['ARCOMSTR'] = "Archiving $TARGET ..."
	env['RANLIBCOMSTR'] = ""
	env['LINKCOMSTR'] = "Linking $TARGET ..."


def createConvenienceLibBuilder(env):
    """This is a utility function that creates the ConvenienceLibrary
    Builder in an Environment if it is not there already.

    If it is already there, we return the existing one.
    
    Based on the stock StaticLibrary and SharedLibrary builders.
    """

    try:
        convenience_lib = env['BUILDERS']['ConvenienceLibrary']
    except KeyError:
        action_list = [ SCons.Action.Action("$ARCOM", "$ARCOMSTR") ]
        if env.Detect('ranlib'):
            ranlib_action = SCons.Action.Action("$RANLIBCOM", "$RANLIBCOMSTR")
            action_list.append(ranlib_action)

        convenience_lib = SCons.Builder.Builder(action = action_list,
                                  emitter = '$LIBEMITTER',
                                  prefix = '$LIBPREFIX',
                                  suffix = '$LIBSUFFIX',
                                  src_suffix = '$SHOBJSUFFIX',
                                  src_builder = 'SharedObject')
        env['BUILDERS']['ConvenienceLibrary'] = convenience_lib
        env['BUILDERS']['Library'] = convenience_lib

    return convenience_lib


def generate(env):
	"""Common environment generation code"""
	
	# FIXME: this is already too late
	#if env.get('quiet', False):
	#	quietCommandLines(env)
	
	# shortcuts
	debug = env['debug']
	machine = env['machine']
	platform = env['platform']
	x86 = env['machine'] == 'x86'
	gcc = env['platform'] in ('linux', 'freebsd', 'darwin')
	msvc = env['platform'] in ('windows', 'winddk', 'wince')

	# Tool
	if platform == 'winddk':
		env.Tool('winddk')
	elif platform == 'wince':
		env.Tool('evc')
	else:
		env.Tool('default')

	# Put build output in a separate dir, which depends on the current 
	# configuration. See also http://www.scons.org/wiki/AdvancedBuildExample
	build_topdir = 'build'
	build_subdir = env['platform']
	if env['dri']:
		build_subdir += "-dri"
	if env['llvm']:
		build_subdir += "-llvm"
	if env['machine'] != 'generic':
		build_subdir += '-' + env['machine']
	if env['debug']:
		build_subdir += "-debug"
	if env['profile']:
		build_subdir += "-profile"
	build_dir = os.path.join(build_topdir, build_subdir)
	# Place the .sconsign file in the build dir too, to avoid issues with 
	# different scons versions building the same source file
	env['build'] = build_dir
	env.SConsignFile(os.path.join(build_dir, '.sconsign'))

	# C preprocessor options
	cppdefines = []
	if debug:
		cppdefines += ['DEBUG']
	else:
		cppdefines += ['NDEBUG']
	if env['profile']:
		cppdefines += ['PROFILE']
	if platform == 'windows':
		cppdefines += [
			'WIN32', 
			'_WINDOWS', 
			'_UNICODE',
			'UNICODE',
			# http://msdn2.microsoft.com/en-us/library/6dwk3a1z.aspx,
			'WIN32_LEAN_AND_MEAN',
			'VC_EXTRALEAN', 
			'_CRT_SECURE_NO_DEPRECATE',
		]
		if debug:
			cppdefines += ['_DEBUG']
	if platform == 'winddk':
		# Mimic WINDDK's builtin flags. See also:
		# - WINDDK's bin/makefile.new i386mk.inc for more info.
		# - buildchk_wxp_x86.log files, generated by the WINDDK's build
		# - http://alter.org.ua/docs/nt_kernel/vc8_proj/
		cppdefines += [
			('_X86_', '1'), 
			('i386', '1'), 
			'STD_CALL', 
			('CONDITION_HANDLING', '1'),
			('NT_INST', '0'), 
			('WIN32', '100'),
			('_NT1X_', '100'),
			('WINNT', '1'),
			('_WIN32_WINNT', '0x0501'), # minimum required OS version
			('WINVER', '0x0501'),
			('_WIN32_IE', '0x0603'),
			('WIN32_LEAN_AND_MEAN', '1'),
			('DEVL', '1'),
			('__BUILDMACHINE__', 'WinDDK'),
			('FPO', '0'),
		]
		if debug:
			cppdefines += [('DBG', 1)]
	if platform == 'wince':
		cppdefines += [
			('_WIN32_WCE', '500'), 
			'WCE_PLATFORM_STANDARDSDK_500',
			'_i386_',
			('UNDER_CE', '500'),
			'UNICODE',
			'_UNICODE',
			'_X86_',
			'x86',
			'_USRDLL',
			'TEST_EXPORTS' ,
		]
	if platform == 'windows':
		cppdefines += ['PIPE_SUBSYSTEM_WINDOWS_USER']
	if platform == 'winddk':
		cppdefines += ['PIPE_SUBSYSTEM_WINDOWS_DISPLAY']
	if platform == 'wince':
		cppdefines += ['PIPE_SUBSYSTEM_WINDOWS_CE']
	env.Append(CPPDEFINES = cppdefines)

	# C preprocessor includes
	if platform == 'winddk':
		env.Append(CPPPATH = [
			env['SDK_INC_PATH'],
			env['DDK_INC_PATH'],
			env['WDM_INC_PATH'],
			env['CRT_INC_PATH'],
		])

	# C compiler options
	cflags = []
	if gcc:
		if debug:
			cflags += ['-O0', '-g3']
		else:
			cflags += ['-O3', '-g3']
		if env['profile']:
			cflags += ['-pg']
		if env['machine'] == 'x86':
			cflags += ['-m32']
		if env['machine'] == 'x86_64':
			cflags += ['-m64']
		cflags += [
			'-Wall', 
			'-Wmissing-prototypes',
			'-Wno-long-long',
			'-ffast-math',
			'-pedantic',
			'-fmessage-length=0', # be nice to Eclipse 
		]
	if msvc:
		# See also:
		# - http://msdn2.microsoft.com/en-us/library/y0zzbyt4.aspx
		# - cl /?
		if debug:
			cflags += [
			  '/Od', # disable optimizations
			  '/Oi', # enable intrinsic functions
			  '/Oy-', # disable frame pointer omission
			]
		else:
			cflags += [
			  '/Ox', # maximum optimizations
			  '/Oi', # enable intrinsic functions
			  '/Os', # favor code space
			]
		if env['profile']:
			cflags += [
				'/Gh', # enable _penter hook function
				'/GH', # enable _pexit hook function
			]
		cflags += [
			'/W3', # warning level
			#'/Wp64', # enable 64 bit porting warnings
		]
		if platform == 'windows':
			cflags += [
				# TODO
			]
		if platform == 'winddk':
			cflags += [
				'/Zl', # omit default library name in .OBJ
				'/Zp8', # 8bytes struct member alignment
				'/Gy', # separate functions for linker
				'/Gm-', # disable minimal rebuild
				'/WX', # treat warnings as errors
				'/Gz', # __stdcall Calling convention
				'/GX-', # disable C++ EH
				'/GR-', # disable C++ RTTI
				'/GF', # enable read-only string pooling
				'/G6', # optimize for PPro, P-II, P-III
				'/Ze', # enable extensions
				'/Gi-', # disable incremental compilation
				'/QIfdiv-', # disable Pentium FDIV fix
				'/hotpatch', # prepares an image for hotpatching.
				#'/Z7', #enable old-style debug info
			]
		if platform == 'wince':
			cflags += [
				'/Gs8192',
				'/GF', # enable read-only string pooling
			]
		# Automatic pdb generation
		# See http://scons.tigris.org/issues/show_bug.cgi?id=1656
		env.EnsureSConsVersion(0, 98, 0)
		env['PDB'] = '${TARGET.base}.pdb'
	env.Append(CFLAGS = cflags)
	env.Append(CXXFLAGS = cflags)

	# Linker options
	linkflags = []
	if gcc:
		if env['machine'] == 'x86':
			linkflags += ['-m32']
		if env['machine'] == 'x86_64':
			linkflags += ['-m64']
	if platform == 'winddk':
		# See also:
		# - http://msdn2.microsoft.com/en-us/library/y0zzbyt4.aspx
		linkflags += [
			'/merge:_PAGE=PAGE',
			'/merge:_TEXT=.text',
			'/section:INIT,d',
			'/opt:ref',
			'/opt:icf',
			'/ignore:4198,4010,4037,4039,4065,4070,4078,4087,4089,4221',
			'/incremental:no',
			'/fullbuild',
			'/release',
			'/nodefaultlib',
			'/wx',
			'/debug',
			'/debugtype:cv',
			'/version:5.1',
			'/osversion:5.1',
			'/functionpadmin:5',
			'/safeseh',
			'/pdbcompress',
			'/stack:0x40000,0x1000',
			'/driver', 
			'/align:0x80',
			'/subsystem:native,5.01',
			'/base:0x10000',
			
			'/entry:DrvEnableDriver',
		]
	env.Append(LINKFLAGS = linkflags)

	# Convenience Library Builder
	createConvenienceLibBuilder(env)
	
	# for debugging
	#print env.Dump()


def exists(env):
	return 1