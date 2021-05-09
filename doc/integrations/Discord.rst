# Why??

Beyond integrating CORTX with super cool communities on Discord, integrating Discord with CORTX solves an issue that many Discord users face. It's hard to easily access files
that users have uploaded to their Discord guilds (aka Disord servers - I just wanted to make the distinction between regular servers and discord servers :)).

Currently, Discord users must scrub through the entire history of a server to search for a specific file. Using CORTX and ElasticSearch, we instead provide a Discord Bot for
users to easily search and store files. In the future, we'd also like to implement some sort of permission tracking.


## File Syncing and Data Backup with Discord

Store-Cortx (The Discord Bot) enables users to access files in your S3 bucket directly from Discord using commands like `!search [filename]` or `!search` to pull up a menu of items in the `testbucket`.

Whenever a new file is shared on any public channel it is automatically added to the Cortx S3 test bucket, ensuring that all your slack files are safe in case a teammate accidently deletes a file that you need.
Conversely, this also means that files stored on the CORTX bucket might be deleted directly.

## File Searching
When we don't know the name of the file, we have to efficiently tell whether the file already exists in the bucket.
We also don't want to query the bucket many times in short succession.

To enable faster indexing of all the files on the S3 bucket, we use Elasticsearch between the Discord Bot and the S3 bucket. A user can find any file using the `!search` command which pulls up a tiny menu.
Users can use the menu to find their files.

This bot is currently under development here:
[Store-CORTX](https://github.com/dhrumilp15/Store-CORTX)
