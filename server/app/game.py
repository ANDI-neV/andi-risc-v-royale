import time

class Player:
    duration = 10
    def __init__(self, name, token):
        self.name = name
        self.token = token
        self.timeout = time.time() + self.duration
    def resetTimeout(self):
        self.timeout = time.time() + self.duration
    def isTimeout(self):
        return time.time() > self.timeout
    
    
Games = []
Players = []




class Board:
    def __init__(self, owner):
        for i in range(10):
            for j in range(10):
                self.board[i][j] = "W"
    def placeShips(self, ships):
        # ship = "{'l': 5, 'x': 0, 'y': 0, 'd': 0}"
        for ship in ships:
            for i in range(ship["l"]):
                if ship["d"] == 0:
                    if self.board[ship["x"] + i][ship["y"]] == "S":
                        return False
                    self.board[ship["x"] + i][ship["y"]] = "S"
                else:
                    if self.board[ship["x"]][ship["y"] + i] == "S":
                        return False
                    self.board[ship["x"]][ship["y"] + i] = "S"
        return True
    def __str__(self):
        board = ""
        for i in range(10):
            for j in range(10):
                board += self.board[i][j]
        return board

class Game:
    def __init__(self, players):
        self.boards = [Board(player) for player in players]
        self.players = players
        self.turn = self.players[0]
    def placeShips(self, player, ships):
        return self.boards[player].placeShips(ships)
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
        



def timeout_cleaner():
    while True:
        time.sleep(1)
        for player in Players:
            if player.isTimeout():
                Players.remove(player)
                break