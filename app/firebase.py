import firebase_admin
from firebase_admin import credentials, storage
def run():
    if not firebase_admin._apps:
        cred = credentials.Certificate("ServerFirebaseKeys.json")
        firebase_admin.initialize_app(cred, {
            'storageBucket': 'image-base64.firebasestorage.app'  # usually ends with appspot.com
        })

