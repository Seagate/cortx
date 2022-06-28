import os
import sys
from dotenv import load_dotenv

load_dotenv()

# PATH vars

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
root = lambda *x: os.path.join(BASE_DIR, *x)

sys.path.insert(0, root("apps"))


# SECURITY WARNING: keep the secret key used in production secret!
SECRET_KEY = "CHANGE THIS!!!"

# SECURITY WARNING: don't run with debug turned on in production!
DEBUG = True
IN_TESTING = sys.argv[1:2] == ["test"]

ALLOWED_HOSTS = []

# Application definition

INSTALLED_APPS = [
    "django.contrib.admin",
    "django.contrib.auth",
    "django.contrib.contenttypes",
    "django.contrib.sessions",
    "django.contrib.messages",
    "django.contrib.staticfiles",
    "storages",
]

PROJECT_APPS = ["fileupload"]

INSTALLED_APPS += PROJECT_APPS

MIDDLEWARE = [
    "django.middleware.security.SecurityMiddleware",
    "django.contrib.sessions.middleware.SessionMiddleware",
    "django.middleware.common.CommonMiddleware",
    "django.middleware.csrf.CsrfViewMiddleware",
    "django.contrib.auth.middleware.AuthenticationMiddleware",
    "django.contrib.messages.middleware.MessageMiddleware",
    "django.middleware.clickjacking.XFrameOptionsMiddleware",
]

ROOT_URLCONF = "{{cookiecutter.repo_name}}.urls"

# Python dotted path to the WSGI application used by Django's runserver.
WSGI_APPLICATION = "{{cookiecutter.repo_name}}.wsgi.application"

# Database

DATABASES = {
    "default": {
        "ENGINE": "django.db.backends.postgresql_psycopg2",
        "NAME": "{{cookiecutter.repo_name}}",
        "USER": "postgres",
        "PASSWORD": "",
        "HOST": "",  # Empty for localhost through domain sockets or '127.0.0.1' for localhost through TCP.
        "PORT": "",  # Set to empty string for default.
    }
}

# Internationalization

LANGUAGE_CODE = "en-gb"

TIME_ZONE = "UTC"

USE_I18N = False

USE_L10N = True

USE_TZ = True


# Static files (CSS, JavaScript, Images)

STATIC_URL = "/static/"


# Additional locations of static files

STATICFILES_DIRS = (root("assets"),)

TEMPLATES = [
    {
        "BACKEND": "django.template.backends.django.DjangoTemplates",
        "APP_DIRS": True,
        "DIRS": [
            root("templates"),
        ],
        "OPTIONS": {
            "debug": DEBUG,
            "context_processors": [
                "django.contrib.auth.context_processors.auth",
                "django.template.context_processors.request",
                "django.template.context_processors.debug",
                "django.template.context_processors.i18n",
                "django.template.context_processors.media",
                "django.template.context_processors.static",
                "django.template.context_processors.tz",
                "django.contrib.messages.context_processors.messages",
            ],
        },
    }
]

# Password validation

AUTH_PASSWORD_VALIDATORS = [
    {
        "NAME": "django.contrib.auth.password_validation.UserAttributeSimilarityValidator",
    },
    {
        "NAME": "django.contrib.auth.password_validation.MinimumLengthValidator",
    },
    {
        "NAME": "django.contrib.auth.password_validation.CommonPasswordValidator",
    },
    {
        "NAME": "django.contrib.auth.password_validation.NumericPasswordValidator",
    },
]


# .local.py overrides all the common settings.
try:
    from .local import *  # noqa
except ImportError:
    pass


# importing test settings file if necessary
if IN_TESTING:
    from .testing import *  # noqa


import os

DEFAULT_FILE_STORAGE = "storages.backends.s3boto3.S3Boto3Storage"
AWS_ACCESS_KEY_ID = os.getenv("ACCESS_KEY_ID")
AWS_SECRET_ACCESS_KEY = os.getenv("SECRET_ACCESS_KEY")
AWS_STORAGE_BUCKET_NAME = os.getenv("BUCKET_NAME")
AWS_S3_ENDPOINT_URL = os.getenv("CORTX_ENDPOINT_URL")
AWS_S3_OBJECT_PARAMETERS = {"CacheControl": "max-age=86400"}
AWS_DEFAULT_ACL = None
AWS_S3_SECURE_URLS = False
AWS_S3_VERIFY = False
AWS_QUERYSTRING_AUTH = False
