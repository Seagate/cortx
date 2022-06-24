import { Button, StyledButton } from "@nextui-org/react";
import { listBucket } from "../../lib/s3Util";




export function S3React(){

    function handleClick(event) {

        listBucket()
    }

    return (
        <StyledButton
        onClick={handleClick}
        >
            ListAWS
        </StyledButton>
    )
}
