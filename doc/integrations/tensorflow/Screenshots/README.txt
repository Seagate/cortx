Read Me

Hello, my name is Bari Arviv.
My idea was to perform an integration from Sypder (Anaconda) to S3.
I did this using code in Python and Tensorflow. I choose TensorFlow because I am currently doing my final project in Deep Learning and the idea really interested me, I am very glad that I took part in the hackathon. I learned a lot of new things and really enjoyed it! thank you very much!

In Shawee there are 2 videos:
The first video explains the Concept Details.
The second video is divided into two parts: one before the run and the other after the run.
Before running you can see that the bucket finally_mnist does not exist yet.

The steps performed in the code:
1.	Opens a new bucket for the current run (finaly_mnist).
2.	Upload a file containing the MNIST database to it.
3.	Download the file from S3 to my computer to drive C.
4.	Upload the database to the model, model training.
5.	Save the model to my computer on drive C.
6.	Uploading the model saved and the model directory is compressed as a zip file to S3.

After running, you can see that the bucket named finally_mnist has been created and has 3 files:
1.	mnist dataset file.
2.	saved_model- This is a file where the trained model is saved.
3.	A zip file of the directory where the model is stored.

Thank you again!
