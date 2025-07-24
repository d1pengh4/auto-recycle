from flask import Flask, request, jsonify
from flask_cors import CORS
from transformers import pipeline
from PIL import Image
import io
import base64

app = Flask(__name__)
CORS(app)

# Trash-Net 모델 파이프라인 로드
pipe = pipeline("image-classification", model="prithivMLmods/Trash-Net")

@app.route('/classify', methods=['POST'])
def classify():
    data = request.json
    img_b64 = data['image']
    img_bytes = base64.b64decode(img_b64)
    img = Image.open(io.BytesIO(img_bytes)).convert('RGB')
    result = pipe(img)
    return jsonify(result)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5005) 