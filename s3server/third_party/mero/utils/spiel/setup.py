from distutils.core import setup, Extension

mero = Extension('mero',
                 define_macros=[('M0_INTERNAL', ''), ('M0_EXTERN', 'extern')],
                 include_dirs=['../../', '../../extra-libs/galois/include/'],
                 sources=['mero.c'],
                 extra_compile_args=['-w'])


setup(name='mero', version='1.0',
      description='Auxiliary definitions used by m0spiel',
      author='Igor Perelyotov', author_email='<igor.m.perelyotov@seagate.com>',
      ext_modules=[mero])
