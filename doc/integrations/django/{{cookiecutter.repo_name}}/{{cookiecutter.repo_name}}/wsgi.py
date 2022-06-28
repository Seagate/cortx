"""
WSGI config for {{cookiecutter.repo_name}} project.

It exposes the WSGI callable as a module-level variable named ``application``.
"""
import os
from os.path import abspath, dirname
from sys import path

SITE_ROOT = dirname(dirname(abspath(__file__)))
path.append(SITE_ROOT)


os.environ.setdefault("DJANGO_SETTINGS_MODULE", "{{cookiecutter.repo_name}}.settings")


from django.core.wsgi import get_wsgi_application

application = get_wsgi_application()
