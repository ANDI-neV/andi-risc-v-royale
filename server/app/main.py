from flask import Flask, request, jsonify
import random
import time
import os
from game import *
import threading

app = Flask(__name__)


playerqueue = []

@app.route('/activity', methods=['GET', 'POST'])
def reply():
    if request.method == 'GET':
        print(request.args)
        return jsonify({"message": "Hello from GET!"})
    elif request.method == 'POST':
        data = request.json
        print(data)
        reply = jsonify({"message": "Received from POST", "data": data})
        print("Replying with", reply)
        return reply


# {"name": char[4]}
@app.route('/register', methods=['POST'])
def register():
    print(request.json)
    name = request.json["name"]
    print(name + " has registered!")
    token = os.urandom(4).hex()
    Players.append(Player(name, token))
    print("Returning token " + token)
    return jsonify({"token": token})

@app.route('/state', methods=['POST'])
def state():
    print(request.json)
    name = request.json["name"]
    token = request.json["token"]
    exists = False
    for player in Players:
        if player.name == name and player.token == token:
            player.resetTimeout()
            exists = True
    if exists:
        for game in Games:
            if name in game.players:
                if game.turn == name:
                    return jsonify({"state": "turn"})
                return jsonify({"state": "play"})
        if player in playerqueue:
            return jsonify({"state": "wait"})
        return jsonify({"state": "idle"})
    return jsonify({"state": "fail"})
            
            

@app.route('/board', methods=['POST'])
def board():
    name = request.json["name"]
    token = request.json["token"]
    for player in Players:
        if player.name == name and player.token == token:
            player.resetTimeout()
            for game in Games:
                if name in game.players:
                    print("Board " + game.boards[game.players.index(name)])
                    return jsonify({"board": game.boards[game.players.index(name)]})
        
        



@app.route('/start', methods=['POST'])
def start():
    name = request.json["name"]
    token = request.json["token"]
    for player in Players:
        if player.name == name and player.token == token:
            Player.get_queue().append(player)
            if len(Player.get_queue()) == 2:
                Games.append(Game(Player.get_queue()))
                Player.get_queue().clear()
            return jsonify({"state": "success"})
    return jsonify({"state": "failure"})

@app.route('/play', methods=['POST'])
def play():
    # The player plays a move
    pass


if __name__ == '__main__':
    # set random seed
    random.seed(time.time())
    cleaner = threading.Thread(target=timeout_cleaner)
    cleaner.start()
    app.run(debug=True, port=11449, host="0.0.0.0")