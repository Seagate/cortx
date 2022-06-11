from .base import *  # noqa
import os


DEBUG = False
TEMPLATES["OPTIONS"]["debug"] = DEBUG

SECRET_KEY = os.environ["SECRET_KEY"]

ALLOWED_HOSTS = os.environ.get("ALLOWED_HOSTS", "localhost").split(",")

ADMINS = (("{{cookiecutter.author_name}}", "{{cookiecutter.email}}"),)

MANAGERS = ADMINS

DATABASES = {
    "default": {
        "ENGINE": "django.db.backends.postgresql_psycopg2",
        "NAME": "{{cookiecutter.repo_name}}",
        "USER": os.environ["DB_USER"],
        "PASSWORD": os.environ.get("DB_PASSWORD", ""),
        "HOST": os.environ.get("DB_HOST", ""),
        "PORT": "",
    }
}
