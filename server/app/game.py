import time
import json

class Player:
    playerqueue = []
    duration = 10
    def __init__(self, name, token):
        self.name = name
        self.token = token
        self.timeout = time.time() + self.duration
    def resetTimeout(self):
        self.timeout = time.time() + self.duration
    def isTimeout(self):
        return time.time() > self.timeout
    def addBoard(self, board, ships):
        self.board = board
        self.board.placeShips(ships)
    @staticmethod
    def get_queue():
        return Player.playerqueue

    


Players = []




class Board:
    def __init__(self, owner):
        self.board = [["W" for i in range(10)] for j in range(10)]
    def placeShips(self, ships):
        # ship = "{'carrier': {'l': 5, 'x': 0, 'y': 0, 'd': 0}"
        for ship in ships.values():
            
            #json_ships = json.loads(ship)
            values = ship#.values()
            for i in range(values["l"]):
                if ship["d"] == 0:
                    if self.board[values["x"] + i][values["y"]] == "S":
                        return False
                    self.board[values["x"] + i][values["y"]] = "S"
                else:
                    if self.board[values["x"]][values["y"] + i] == "S":
                        return False
                    self.board[values["x"]][values["y"] + i] = "S"
        return True
    def __str__(self):
        board = ""
        for i in range(10):
            for j in range(10):
                board += self.board[i][j]
        return board

class Game:
    Games = []
    def __init__(self, players):
        self.players = players
        self.turn = self.players[0]
    def play(self, player, x, y):
        self.turn = self.players[(self.players.index(player) + 1) % 2]
        if self.boards[player].board[x][y] == "S":
            self.boards[player].board[x][y] = "H"
            return "H"
        elif self.boards[player].board[x][y] == "W":
            self.boards[player].board[x][y] = "M"
            return "M"
        else:
            return "E"
    @staticmethod
    def getGames():
        return Game.Games
    def __str__(self):
        boardstring = ""
        boardstring += self.players[0].board.__str__()
        boardstring += self.players[1].board.__str__()
        return boardstring
        



def timeout_cleaner():
    while True:
        time.sleep(1)
        for player in Players:
            if player.isTimeout():
                Players.remove(player)
                break