from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route('/activity', methods=['GET', 'POST'])
def reply():
    if request.method == 'GET':
        return jsonify({"message": "Hello from GET!"})
    elif request.method == 'POST':
        data = request.json
        return jsonify({"message": "Received from POST", "data": data})

if __name__ == '__main__':
    app.run(debug=True)