import time

class Player:
    duration = 10*1000
    def __init__(self, name, token, timeout):
        self.name = name
        self.token = token
        self.timeout = time.time() + timeout
    def resetTimeout(self):
        self.timeout = time.time() + self.duration
    def isTimeout(self):
        return time.time() > self.timeout
    
    
Players = []

class Game:
    def __init__(self):
        for i in range(10):
            for j in range(10):
                self.board[i][j] = "0"
    def placeShips(self):
        pass

def timeout_cleaner():
    while True:
        time.sleep(1)
        for player in Players:
            if player.isTimeout():
                print(player.name + " has timed out!")
                Players.remove(player)
                break