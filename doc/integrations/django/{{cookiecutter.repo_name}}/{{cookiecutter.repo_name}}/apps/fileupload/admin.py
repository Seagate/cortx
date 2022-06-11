from django.contrib import admin
from .models import FileUpload


class FileUploadAdmin(admin.ModelAdmin):
    pass


admin.site.register(FileUpload, FileUploadAdmin)
