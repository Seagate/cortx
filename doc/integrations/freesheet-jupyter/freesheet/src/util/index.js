import { DATA_FILE_NAME } from "./constants";

export const formatMoney = (m) => {
  var formatter = new Intl.NumberFormat("en-US", {
    style: "currency",
    currency: "USD",

    // These options are needed to round to whole numbers if that's what you want.
    //minimumFractionDigits: 0, // (this suffices for whole numbers, but will print 2500.10 as $2,500.1)
    //maximumFractionDigits: 0, // (causes 2500.99 to be printed as $2,501)
  });
  return formatter.format(m);
};

export const getFileName = (fullPath) => fullPath.replace(/^.*[\\\/]/, '');

export function getAverage(array) {
  return array.reduce((a, b) => a + b) / array.length;
}

const createIpfsCode = (ipfsUrl) => {
  return {
    "cells": [
     {
      "cell_type": "code",
      "execution_count": null,
      "id": "a2004048-673e-4424-8770-744df428d9a7",
      "metadata": {},
      "outputs": [],
      "source": [
       "import pandas as pd\n",
       `INPUT_URL = '${ipfsUrl}'`
      ]
     },
     {
      "cell_type": "code",
      "execution_count": null,
      "id": "b38eaa74-7fce-4b79-b644-345e4b4c850f",
      "metadata": {},
      "outputs": [],
      "source": [
       "df = pd.read_csv(INPUT_URL)"
      ]
     },
     {
      "cell_type": "code",
      "execution_count": null,
      "id": "e2e11e9d-1924-46d9-8da3-b4dccba0c627",
      "metadata": {},
      "outputs": [],
      "source": [
       "df.info()"
      ]
     },
     {
      "cell_type": "code",
      "execution_count": null,
      "id": "ea07e7d4-605c-4b25-8465-182a7036c55b",
      "metadata": {},
      "outputs": [],
      "source": [
       "df.head()"
      ]
     },
     {
      "cell_type": "code",
      "execution_count": null,
      "id": "3156ba79-1ae8-4e4c-afb7-dadbe0c0c33c",
      "metadata": {},
      "outputs": [],
      "source": []
     }
    ],
    "metadata": {
     "kernelspec": {
      "display_name": "python3.9",
      "language": "python",
      "name": "python3.9"
     },
     "language_info": {
      "codemirror_mode": {
       "name": "ipython",
       "version": 3
      },
      "file_extension": ".py",
      "mimetype": "text/x-python",
      "name": "python",
      "nbconvert_exporter": "python",
      "pygments_lexer": "ipython3",
      "version": "3.9.12"
     }
    },
    "nbformat": 4,
    "nbformat_minor": 5
   }
}

export const downloadNotebookFile = async (ipfsUrl) => {
  const codeObj = createIpfsCode(ipfsUrl)
  const element = document.createElement("a");
  const file = new Blob([JSON.stringify(codeObj)], {
    type: "text/plain"
  });
  element.href = URL.createObjectURL(file);
  element.download = "freesheet.ipynb";
  document.body.appendChild(element);
  element.click();
};
