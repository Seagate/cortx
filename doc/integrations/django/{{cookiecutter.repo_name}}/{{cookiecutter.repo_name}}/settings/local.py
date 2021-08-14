from .base import *  # noqa


ADMINS = (("{{cookiecutter.author_name}}", "{{cookiecutter.email}}"),)

MANAGERS = ADMINS

DATABASES = {
    "default": {
        "ENGINE": "django.db.backends.sqlite3",
        "NAME": "db.sqlite3",
    }
}


# You might want to use sqlite3 for testing in local as it's much faster.
if IN_TESTING:
    DATABASES = {
        "default": {
            "ENGINE": "django.db.backends.sqlite3",
            "NAME": "/tmp/{{cookiecutter.repo_name}}_test.db",
        }
    }
