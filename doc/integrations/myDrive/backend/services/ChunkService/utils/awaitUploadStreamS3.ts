import s3 from "../../../db/s3";
import s3Personal from "../../../db/S3Personal";

const awaitUploadStreamS3 = (params: any, personalFile: boolean, s3Data: { id: string, key: string, bucket: string }) => {

    return new Promise((resolve, reject) => {
        console.log("personalFile", personalFile);
        if (personalFile) {

            const s3PersonalAuth = s3Personal(s3Data.id, s3Data.key);

            s3PersonalAuth.upload(params, (err: any, data: any) => {

                if (err) {
                    console.log("Amazon upload personal err", err)
                    reject("Amazon upload error");
                }

                resolve();
            })

        } else {

            const res = s3.upload(params, (err: any, data: any) => {
                console.log("111 s3.upload", err, data);
                if (err) {
                    console.log("Amazon upload err", err)
                    reject("Amazon upload error");
                }

                resolve();
            })

            console.log("2222 s3.upload.res", res);
        }
    })
}

export default awaitUploadStreamS3;
