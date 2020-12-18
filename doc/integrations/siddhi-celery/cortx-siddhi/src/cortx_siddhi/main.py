import sys
import logging
from argparse import ArgumentParser

from .logic import run


logger = logging.getLogger('main')


def main():
    parser = ArgumentParser()
    parser.add_argument('-v', '--verbose', help='Show debug info')
    args = parser.parse_args()
    # logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)
    logging.root.level = logging.DEBUG if args.verbose else logging.INFO
    try:
        run(args)
    except Exception as e:  # pylint: disable=broad-except
        logger.error(e)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
