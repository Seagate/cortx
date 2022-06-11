from django.db import models

# Create your models here.


class FileUpload(models.Model):
    name = models.CharField(max_length=50)
    file = models.FileField()
