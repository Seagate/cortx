(class extends VDMApplet {
    constructor(appletProfile) {
        super(appletProfile);
        let myApp = this;

        // Dropdown menu items
        myApp.menu = {
        };

        myApp.appFuncs = {
        };

        myApp.appVars = {
            "videoURL": "https://www.youtube.com/embed/dQw4w9WgXcQ?rel=0&amp;autoplay=1&amp;controls=0&amp;showinfo=0"
        };

        myApp.recvCmd = {
        };

    }

    runStartup() {
        let myApp = this;
        myApp.windowParts["data"].innerHTML = '<iframe width="100% " height="100% " src="' + myApp.appVars['videoURL'] + '" frameborder="0" allowfullscreen=""></iframe>';
    }
})
//# sourceURL=vdm-app-RickRoll.js
