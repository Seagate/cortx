import argparse
import csv
import cortx_community


# Get repository from given organization
def get_repo(repo_name, org_name="seagate"):
    g = cortx_community.get_gh()
    org = g.get_organization(org_name)
    return org.get_repo(repo_name)


# Fetch all the open pull requests from the repository
def get_pulls(orrepo):
    return orrepo.get_pulls(state='open', sort='created')


# Count number of PRs per each author
def get_pull_author(pulls):
    users = []
    for pull in pulls:
        users.append(pull.user.login)
    return dict((user, users.count(user)) for user in set(users))


# Create a csv file
def write_to_csv(users, orrepo, total_prs):
    with open('{}_open_pull_request.csv'.format(orrepo.full_name.split("/")[1]), 'w', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=[
                                "Username", "No_of_open_PRs"])
        writer.writeheader()
        for key in users:
            writer.writerow({"Username": key, "No_of_open_PRs": users[key]})
        writer.writerow({"Username": "TOTAL_PRs", "No_of_open_PRs":total_prs})


def main():
    parser = argparse.ArgumentParser(description='Get repository name')
    parser.add_argument('--repo_name',  '-i', help='Github repository to fetch open PRs.')
    args = parser.parse_args()

    orrepo = get_repo(args.repo_name)
    pulls = get_pulls(orrepo)
    users = get_pull_author(pulls)
    write_to_csv(users, orrepo, pulls.totalCount)


if __name__ == "__main__":
    main()
