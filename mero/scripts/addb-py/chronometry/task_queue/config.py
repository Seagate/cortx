from huey import SqliteHuey

huey = SqliteHuey(filename='cluster_queue.db')
