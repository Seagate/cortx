import path from "path";

const getEnvVariables = () => {

    const configPath = path.join(__dirname, "..", "..", "config");

    const processType = process.env.NODE_ENV;

    if (processType === 'production' || processType === undefined) {

        require('dotenv').config({path: configPath + "/prod.env"})

    } else if (processType === 'development') {

        require('dotenv').config({path: configPath + "/dev.env"})

    } else if (processType === 'test') {

        require('dotenv').config({path: configPath + "/test.env"})
    }

    console.log("processType = ", processType);
    console.log("MONGODB_URL",process.env.MONGODB_URL);
}

export default getEnvVariables;
module.exports = getEnvVariables
