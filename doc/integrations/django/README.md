# Django + CORTX Cookiecutter

A simple [cookiecutter](https://github.com/cookiecutter/cookiecutter) template that fuses Django and CORTX as the key object storage solution, into one full stack application.

Integration video link: https://vimeo.com/581760289

### Motivations

Using Cookiecutter, the community is able to automatically spin up a Django project that is integrated with CORTX, with an inbuilt file upload functionality.

With Django as one of the top Python-based web frameworks, cookiecutter templates are able to lower the barrier of entry for integrating with CORTX as the object storage solution of choice. We also hope that future solutions can leverage on this cookiecutter template when they want to integrate CORTX with their Django project.

## Installation

1. Install cookiecutter and cookiecutter-django-cortx:

```
pip install cookiecutter
cd <your-working-directory>
cookiecutter https://github.com/calvinyanghwa/cookiecutter-django-cortx.git
```

2. Install development requirements:

```
pip install -r requirements/local.txt
```

3. Rename the `.env.example` file to `.env` (**IMPORTANT** so that it doesn't get committed into git)

4. Apply migrations:

```
python manage.py migrate
```

5. Create a new admin user:

```
python manage.py createsuperuser
```

6. Run the server:

```
python manage.py runserver
```

Navigate to `127.0.0.1:8000/admin` to access the file upload functionality. Then, feel free to build further on top of the cookiecutter project.

## Contributors

This project was submitted for Seagate CORTX Integration Challenge: Singapore.

- Calvin Yang
- Denise Lee
- Lee Yu Jing
- Mitra Hadesh
- Teo Zhi Wei
