from redis import Redis

HOSTNAME = "localhost"
PORT     = 1337

if __name__ == '__main__':
    r = Redis(HOSTNAME, PORT)

    print("======= REDIS =======")
    print(r.handshake())
    print("=====================")

    r.interactive()