import os
from setuptools import setup, find_packages

def read(fname):
    filename = os.path.join(os.path.dirname(__file__), fname)
    with open(filename, 'r') as f:
        return f.read()


setup_req = []
setup_options = {}


# Deduce version, if possible.
if os.path.isfile('../VERSION'):
    setup_options['version'] = read('../VERSION').strip()
else:
    setup_options['version_config'] =  {
        "version_format": '{tag}.dev+git.{sha}',
        "starting_version": "2019.05.01"
    }
    setup_req.append('better-setuptools-git-version')

setup(
    name='pygreat',
    setup_requires=setup_req,
    url='https://greatscottgadgets.com/greatfet/',
    license='BSD',
    entry_points={
        'console_scripts': [],
    },
    author='Katherine J. Temkin',
    author_email='ktemkin@greatscottgadgets.com',
    tests_require=[''],
    install_requires=['pyusb', 'future', 'backports.functools_lru_cache'],
    description='Python library for talking with libGreat devices',
    long_description=read('../README.md'),
    packages=find_packages(),
    include_package_data=True,
    platforms='any',
    classifiers = [
        'Programming Language :: Python',
        'Development Status :: 1 - Planning',
        'Natural Language :: English',
        'Environment :: Console',
        'Environment :: Plugins',
        'Intended Audience :: Developers',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: BSD License',
        'Operating System :: OS Independent',
        'Topic :: Scientific/Engineering',
        'Topic :: Security',
        ],
    extras_require={},
    **setup_options
)
