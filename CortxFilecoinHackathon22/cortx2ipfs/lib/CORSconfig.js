
export const CORSConfiguration = [
    {
        "AllowedHeaders": [
            "*"
        ],
        "AllowedMethods": [
            "PUT",
            "POST",
            "DELETE"
        ],
        "AllowedOrigins": [
            "http://localhost:3000"
        ],
        "ExposeHeaders": []
    },
    // {
    //     "AllowedHeaders": [
    //         "*"
    //     ],
    //     "AllowedMethods": [
    //         "PUT",
    //         "POST",
    //         "DELETE"
    //     ],
    //     "AllowedOrigins": [
    //         "http://localhost:3000"
    //     ],
    //     "ExposeHeaders": []
    // },
    {
        "AllowedHeaders": [],
        "AllowedMethods": [
            "GET"
        ],
        "AllowedOrigins": [
            "*"
        ],
        "ExposeHeaders": []
    }
]