(class extends VDMApplet {
    constructor(appletProfile) {
        super(appletProfile);
        let myApp = this;

        // Dropdown menu items
        myApp.menu = {
            "General":
                {
                    "Placeholder": function() {
                        let thisDesktop = myApp.vdmDesktop;
                        thisDesktop.newWindow({
                            title: "Blah",
                            sizeX: 400,
                            sizeY: 200
                            /*
                            menu: {
                                "Funcs": {
                                    "Alert": function() { alert("This is a test"); }
                                }
                            }
                            */
                        });
                        let bob = 1;
                    }
                }
        }

        myApp.appFuncs = {
        }

        myApp.appVars = {
        }

        myApp.recvCmd = {
        }

    }

    runStartup() {
        let myApp = this;
        myApp.windowParts["data"].innerHTML = 'This is an empty page';
    }
})
//# sourceURL=vdm-app-Blank.js
