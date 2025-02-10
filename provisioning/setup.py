from setuptools import setup, find_packages

setup(
    name="xluuv",
    version="1.0",
    packages=find_packages(),
    install_requires=[
        "libtmux",
    ],
    entry_points={
        "console_scripts": [
            "xluuv=xluuv.main:entry_point",
        ],
    },
)
