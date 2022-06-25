import unittest

from api import app


class ApiTest(unittest.TestCase):

    def setUp(self):
        self.app = app.test_client()

    def get_image(self):

        response = self.app.get('/images/bg.jpg')

        self.assertEqual(200, response.status_code)

    def tearDown(self):
        pass