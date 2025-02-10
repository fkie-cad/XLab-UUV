from setuptools import setup, find_packages

setup(
    name="xluuv_net",
    version="0.1",
    packages=find_packages(),
    install_requires=[
        "dacite",
        "mininet",
    ],
)
