// VDM Desktop
/**
 * VDM Desktop manager
 * @param {HTMLDivElement} vdmDiv Base DIV for the VDM
 * @param {string} vdmTitle Title on top bar
 * @param {Object.<string,VDMAppletProfile>} appletProfiles Dictionary of applet profiles
 */
class VDMDesktop {
    constructor(vdmDiv, vdmTitle, appletPath) {
        let thisVDMDesktop = this;

        // VDM Desktop DIV
        this.vdmDiv = vdmDiv;

        // VDM Window Array
        this.vdmWindows = [];

        // Current Window
        this.currentWindowDiv = null;

        // App Profiles
        this.appletProfiles = {};

        /** @type {Object.<string,VDMApplet>} */
        this.appletInstances = {};

        // Applet base path
        this.appletPath = appletPath || "vdmapplets";

        // App Resources
        this.loadedResources = [];
        this.sharedJSON = {};

        // Misc
        this.appletCreateIndex = 0;

        // Top Bar
        this.vdmTopBar = {
            title: vdmTitle,
            leftSide: [],
            rightSide: []
        };

        // Top Bar Status LED classes
        this.vdmLEDClasses = {
            red: "led led-red",
            green: "led led-green",
            blue: "led led-blue",
            yellow: "led led-yellow"
        };

        // Disable F5 and backspace keys unless we're in a text input
        this.disableBackAndRefreshKeys();

        // Set vdmDiv height
        $(this.vdmDiv).height($(window).height());

        // Populate vdmDiv
        this.createVDMStructure(this.vdmDiv);

        // Assign major elements
        this.vdmTopBarDiv = $(this.vdmDiv).find(".vdmTopBar")[0];
        this.vdmMenuDiv = $(this.vdmDiv).find(".vdmMenu")[0];
        this.vdmWindowsDiv = $(this.vdmDiv).find(".vdmWindows")[0];

        // Assign TopBarHeight
        this.vdmTopBarHeight = $(this.vdmTopBarDiv).height();

        // Assign topNav
        this.vdmTopNav = $(this.vdmTopBarDiv).find(".topNav")[0];

        // Assign LED
        this.vdmTopBarLEDSpan = $(this.vdmTopBarDiv).find(".led")[0];

        // Assign warning button link and label
        this.alertBtn = $(this.vdmTopNav).find(".btn-group.alerts")[0];
        this.alertAnchor = $(this.alertBtn).find("li")[0];
        this.alertSpan = $(this.alertBtn).find("span")[0];

        // Assign TopBarMenu UL
        this.vdmTopBarMenuUL = $(this.vdmTopBarDiv).find(".dropMenu")[0];
        $(this.vdmTopBarMenuUL).append("<li class=\"nav-last\"></li>");

        this.vdmDiv.style.height = $(window).height() + "px";
        $(this.vdmDiv).show();

        // Resize Window logic
        $(window).resize(function () {
            $(vdmDiv).height($(window).height());
            $(".vdmWindow").each(function (i) {
                let topLevel = $(this).parent();
                topLevel.height($(window).height());
            });
            $(".vdmWindow").each(function (i) {
                $(this).draggable({
                    handle: '.vdmWindowHeader',
                    containment: [0, thisVDMDesktop.vdmTopBarHeight, $(window).width() - 50, $(window).height() - 50]
                });
            });
        });
    }

    disableBackAndRefreshKeys() {
        $(document).bind("keydown", function (e) {
            if ((e.which || e.keyCode) === 116) {
                e.preventDefault();
            }
            if ((e.which || e.keyCode) === 8 && !$(e.target).is("input, textarea")) {
                e.preventDefault();
            }
        });
    }

    createVDMStructure(vdmDiv) {
        let thisVDMDesktop = this;

        $(vdmDiv).append(`
<div class="vdmTopBar">
    <div class="topTitle">
        <div class="titleBar">
            ${thisVDMDesktop.vdmTopBar.title}<span style="display: inline-flex;">&nbsp;&nbsp;</span><span class="led" style="display: inline-flex;"></span>
        </div>
    </div>
    <div class="topVDMStatus"></div>
        <div class="topNav">
            <div class="btn-group alerts">
                <span data-placement="bottom" data-original-title="Alerts" data-toggle="modal" class="btn btn-default btn-sm">
                    <i class="fa fa-warning"></i>
                    <span class="badge badge-success">0</span>
                </span>
            </div>
        </div>
        <div class="topMenu">
        <button class="dropButton">Go <i class="fa fa-chevron-down"></i></button>
        <ul class="dropMenu">
        </ul>
    </div>
</div>
<div class="vdmMenu"/>
<div class="vdmWindows"/>
`);
    }

    changeLEDColor(newColor) {
        let thisVDMDesktop = this;

        if (thisVDMDesktop.vdmLEDClasses[newColor]) {
            thisVDMDesktop.vdmTopBarLEDSpan.className = thisVDMDesktop.vdmLEDClasses[newColor];
            return 0;
        } else {
            return 1;
        }
    }

    // Add Client app profile
    /**
     * @param {VDMAppletProfile} appletProfile Profile describing new Window
     */
    addAppletProfile(appletProfile) {
        let thisVDMDesktop = this;

        // Check to see if we have a name and the necessary attributes
        if (!appletProfile) {
            console.log("Cannot add app - No app definition");
        } else if (!appletProfile.appletName) {
            console.log("Cannot add app - App definition does not contain 'name' parameter");
        } else if (!appletProfile.appletScript) {
            console.log("Cannot add app '" + appletProfile.appletName + "' - App definition does not contain 'appletScript' parameter");
        } else {
            thisVDMDesktop.appletProfiles[appletProfile.appletName] = appletProfile;
        }
    }

    evalWithinContext(context, code) {
        let outerResults = function (code) {
            let innerResults = eval(code);
            return innerResults;
        }.apply(context, [code]);
        return outerResults;
    }

    async loadAppletScripts() {
        let thisVDMDesktop = this;
        let appletProfileList = Object.keys(thisVDMDesktop.appletProfiles);
        for (let i = 0; i < appletProfileList.length; i++) {
            var tmpAppletName = appletProfileList[i];
            var appletDefinition = thisVDMDesktop.appletProfiles[tmpAppletName];
            var tmpScriptPath = appletDefinition.appletScript;
            if (!appletDefinition.appletPath) appletDefinition.appletPath = thisVDMDesktop.appletPath;
            if (!appletDefinition.appletScript.match(/https?:\/\//)) {
                tmpScriptPath = thisVDMDesktop.appletPath + '/' + appletDefinition.appletScript;
                let thisAppletScript = await thisVDMDesktop.fetchURLResource(tmpScriptPath);
                appletDefinition.appletClass = thisVDMDesktop.evalWithinContext(appletDefinition, thisAppletScript);
            }
        }
    }

    async loadAppletProfiles() {
        let thisVDMDesktop = this;
        if (!thisVDMDesktop.loaded) {
            await thisVDMDesktop.loadAppletScripts();
            let profileKeys = Object.keys(thisVDMDesktop.appletProfiles);
            for (let i = 0; i < profileKeys.length; i++) {
                let appKeyName = profileKeys[i];
                let appletProfile = thisVDMDesktop.appletProfiles[appKeyName];
                if (appletProfile.showInMenu) {
                    thisVDMDesktop.addDropDownMenuItem(function () {
                        thisVDMDesktop.openApp(appKeyName, null);
                    }, appletProfile.appletIcon, appletProfile.title);
                }

                if (typeof appletProfile.preReqs !== "undefined" && appletProfile.preReqs.length > 0) {
                    await thisVDMDesktop.loadAppletResources(appletProfile);
                }
            }
            thisVDMDesktop.loaded = true;
        }
    }

    async loadAppletResources(appletProfile) {
        let thisVDMDesktop = this;

        for (let i = 0; i < appletProfile.preReqs.length; i++) {
            let preReqHash = appletProfile.preReqs[i];
            let preReqKeys = Object.keys(preReqHash);
            for (let j = 0; j < preReqKeys.length; j++) {
                let preReqType = preReqKeys[j];
                let preReqLocation = preReqHash[preReqType];

                switch (preReqType) {
                    case 'CSS':
                        if (thisVDMDesktop.loadedResources.indexOf(preReqLocation) === -1) {
                            thisVDMDesktop.loadedResources.push(preReqLocation);

                            // Append it to HEAD
                            let resourceText = await thisVDMDesktop.fetchURLResource(preReqLocation);
                            $("head").append($("<style>" + resourceText + "</style>"));

                        }
                        break;
                    case 'JS':
                        if (thisVDMDesktop.loadedResources.indexOf(preReqLocation) === -1) {
                            thisVDMDesktop.loadedResources.push(preReqLocation);

                            // Run it globally now
                            let resourceText = await thisVDMDesktop.fetchURLResource(preReqLocation);
                            jQuery.globalEval(resourceText);
                        }
                        break;
                    case 'JS-Runtime':

                        // Cache for execution at runtime (executes before runStartup)
                        let resourceText = await thisVDMDesktop.fetchURLResource(preReqLocation);
                        appletProfile.startupScript = resourceText;

                        break;
                    case 'JS-Head':
                        if (thisVDMDesktop.loadedResources.indexOf(preReqLocation) === -1) {
                            thisVDMDesktop.loadedResources.push(preReqLocation);

                            // Run it globally now
                            let script = document.createElement('script');
                            script.src = preReqLocation;
                            script.defer = true;

                            document.head.appendChild(script);
                        }
                        break;
                    case 'JSON':
                        if (thisVDMDesktop.loadedResources.indexOf(preReqLocation) === -1) {
                            thisVDMDesktop.loadedResources.push(preReqLocation);

                            // Cache for use at runtime
                            let resourceText = await thisVDMDesktop.fetchURLResource(preReqLocation);
                            thisVDMDesktop.sharedJSON[preReqLocation] = resourceText;

                        }
                        break;
                    default:
                        alert("Unknown prerequisite type: '" + preReqType + "'");
                        return false;
                }
            }
        }
    }

    fetchURLResource(url) {
        let thisVDMDesktop = this;
        return new Promise(function (resolve, reject) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", url);
            xhr.onload = function () {
                if (this.status >= 200 && this.status < 300) {
                    resolve(xhr.responseText);
                } else {
                    reject({
                        status: this.status,
                        statusText: xhr.statusText
                    });
                }
            };
            xhr.onerror = function () {
                reject({
                    status: this.status,
                    statusText: xhr.statusText
                });
            };
            xhr.send();
        });
    }

    // Add Item to Drop down menu
    addDropDownMenuItem(onClickAction, iconClass, itemLabel) {
        let thisVDMDesktop = this;

        let itemLI = document.createElement("li");
        let itemA = document.createElement("span");
        $(itemA).click(function () { onClickAction(); });
        let itemI = document.createElement("i");
        $(itemI).attr({
            class: "fa " + iconClass
        });
        let itemSpan = document.createElement("span");
        $(itemSpan).attr({
            class: "link-title"
        });
        $(itemSpan).html("&nbsp;" + itemLabel);
        itemA.appendChild(itemI);
        itemA.appendChild(itemSpan);
        itemLI.appendChild(itemA);
        thisVDMDesktop.vdmTopBarMenuUL.append(itemLI);
    }

    async openApp(appletName, parameters) {
        let thisVDMDesktop = this;

        if (appletName !== "") {

            // Get app profile
            var appDefinition = thisVDMDesktop.appletProfiles[appletName];
            if (!appDefinition) {
                console.log("Tried to open non-existent app [" + appletName + "]");
                return null;
            }

            // Create new instance of applet
            let newApp = new appDefinition.appletClass(appDefinition, parameters);

            // Attach window to applet
            await thisVDMDesktop.newWindow(newApp);

            // Add to Applet list
            thisVDMDesktop.appletInstances[newApp.appletIndex] = newApp;
        }
    }

    async newWindow(newAppletObj) {
        let thisVDMDesktop = this;

        // Link back to VDM Desktop
        newAppletObj.vdmDesktop = thisVDMDesktop;

        // The object should contain: Title, Height, Width, Menu, MenuHandler
        // We will add: ParentID, WindowID, StartX, StartY, Index
        // Return: ID
        newAppletObj.desktopDivID = this.vdmWindowsDiv;

        // Revisit this logic.  Starting from first spot, see if any windows are using the spot.  If not, put the window there.  Worst case
        // we'll use this method.  Meaning that if all spots are taken, go in order regardless of spots taken.  This will result in overlap.
        newAppletObj.zIndex = this.vdmWindows.length + 1;
        newAppletObj.appletIndex = this.appletCreateIndex;

        // Increment the window create index
        this.appletCreateIndex++;

        // Set Window ID
        newAppletObj.windowID = 'vdmWindow-' + this.appletCreateIndex;

        // Create new Window DIV
        $(newAppletObj.desktopDivID).append("<div id=\"" + newAppletObj.windowID + "\" class=\"vdmWindow\">");

        // Assign element reference to windowDiv
        newAppletObj.windowDiv = document.getElementById(newAppletObj.windowID);

        // Set position, index, height and width
        $(newAppletObj.windowDiv).css({
            'top': ((thisVDMDesktop.appletCreateIndex & 7) + 1) * 10,
            'left': ((thisVDMDesktop.appletCreateIndex & 7) + 1) * 10,
            'z-index': newAppletObj.zIndex
        });
        $(newAppletObj.windowDiv).width(newAppletObj.sizeX);
        $(newAppletObj.windowDiv).height(newAppletObj.sizeY);

        // See if we have menuItems
        let haveMenuItems = newAppletObj.menu && Object.keys(newAppletObj.menu).length > 0;

        // Add member elements to windowDiv
        $(newAppletObj.windowDiv).append("<div class=\"vdmWindowHeader\"></div>");
        if (haveMenuItems) $(newAppletObj.windowDiv).append("<div class=\"vdmWindowMenu\"></div>");
        $(newAppletObj.windowDiv).append("<div class=\"vdmWindowData\"></div>");
        $(newAppletObj.windowDiv).append("<div class=\"vdmWindowFooter\"></div>");
        $(newAppletObj.windowDiv).append("<div class=\"vdmWindowPopover\"><div class=\"popoverbox\" tabindex=0></div></div>");

        // Assign elements to windowObj
        newAppletObj.windowParts = {
            "header": $(newAppletObj.windowDiv).children(".vdmWindowHeader")[0],
            "menu": $(newAppletObj.windowDiv).children(".vdmWindowMenu")[0],
            "data": $(newAppletObj.windowDiv).children(".vdmWindowData")[0],
            "footer": $(newAppletObj.windowDiv).children(".vdmWindowFooter")[0],
            "popover": $(newAppletObj.windowDiv).children(".vdmWindowPopover")[0]
        };

        if (!haveMenuItems) {
            newAppletObj.windowParts.data.style.top = "18px";
        }

        // If we have an HTML template file, retrieve and copy to the data window
        if (typeof newAppletObj.htmlFile !== "undefined") {
            let tgtDiv = newAppletObj.windowParts["data"];
            let resourceText = await thisVDMDesktop.fetchURLResource(newAppletObj.htmlFile);
            $(tgtDiv).html(resourceText);
        }

        // Create Maximize button
        let ctrlMaximize = document.createElement("span");
        ctrlMaximize.onclick = async function () {
            let elem = newAppletObj.windowParts.data;

            // Define fullscreen exit handler
            let exitHandler = () => {
                if (elem.requestFullscreen) {
                    document.removeEventListener('fullscreenchange', exitHandler, false);
                } else if (elem.webkitRequestFullscreen) { /* Safari */
                    document.removeEventListener('webkitfullscreenchange', exitHandler, false);
                } else if (elem.msRequestFullscreen) { /* IE11 */
                    document.removeEventListener('msfullscreenchange', exitHandler, false);
                }

                // Call resizing hook if set
                if (typeof newAppletObj.resizeMovingHook !== "undefined") {
                    newAppletObj.resizeMovingHook();
                }
            }

            // Execute relevant fullscreen request
            if (elem.requestFullscreen) {
                await elem.requestFullscreen();
            } else if (elem.webkitRequestFullscreen) { /* Safari */
                await elem.webkitRequestFullscreen();
            } else if (elem.msRequestFullscreen) { /* IE11 */
                await elem.msRequestFullscreen();
            }

            // Insert delay.  The following event listeners were firing prematurely
            // when called immediately after the requestFullscreen functions.  Works
            // as low as 1ms on Skylake PC, but upping to 100 in case slower systems
            // need the extra time.
            await new Promise(res => setTimeout(res, 100));

            // Add fullscreenchange event listener so we know when to resize
            if (elem.requestFullscreen) {
                document.addEventListener('fullscreenchange', exitHandler, false);
            } else if (elem.webkitRequestFullscreen) { /* Safari */
                document.addEventListener('webkitfullscreenchange', exitHandler, false);
            } else if (elem.msRequestFullscreen) { /* IE11 */
                document.addEventListener('msfullscreenchange', exitHandler, false);
            }

            // Call resizing hook if set
            if (typeof newAppletObj.resizeMovingHook !== "undefined") {
                newAppletObj.resizeMovingHook();
            }
        };
        $(ctrlMaximize).append("<i class=\"fa fa-square-o fa-lg\" style=\"font-weight: bold; color: #c3af73; top: 2px; right: 5px; position: relative;\"></i>");

        // Create Close button
        let ctrlClose = document.createElement("span");
        ctrlClose.onclick = function () { thisVDMDesktop.closeWindow(newAppletObj); };
        $(ctrlClose).append("<i class=\"fa fa-times fa-lg\" style=\"top: 1px; right: 0px; position: relative;\"></i>");

        // Add title and close button to the header
        $(newAppletObj.windowDiv).children(".vdmWindowHeader").append(
            "<span class=\"title\">" + newAppletObj.title + "</span>" +
            "<span class=\"ctrls\"></span>"
        );
        $(newAppletObj.windowDiv).children(".vdmWindowHeader").children(".ctrls").append(ctrlMaximize);
        $(newAppletObj.windowDiv).children(".vdmWindowHeader").children(".ctrls").append(ctrlClose);

        // If we have a Startup Script, run it
        if (newAppletObj.appletName && thisVDMDesktop.appletProfiles[newAppletObj.appletName] && thisVDMDesktop.appletProfiles[newAppletObj.appletName].startupScript !== '') {
            thisVDMDesktop.evalWithinContext(newAppletObj, thisVDMDesktop.appletProfiles[newAppletObj.appletName].startupScript);
        }

        // Create and populate menu element
        let mainMenu = document.createElement("ul");
        $.each(newAppletObj.menu, function (menuHeader, menuItems) {
            let menuCol = document.createElement("li");
            let menuTop = $('<span/>', { tabindex: "1" });
            $(menuTop).append(menuHeader);
            let menuOptionList = document.createElement("ul");
            $.each(menuItems, function (optionName, optionValue) {
                let optRef = document.createElement("span");
                optRef.onclick = optionValue;
                $(optRef).append(optionName);
                let optLi = document.createElement("li");
                optLi.appendChild(optRef);
                menuOptionList.appendChild(optLi);
            });
            $(menuCol).append(menuTop);
            $(menuCol).append(menuOptionList);
            $(mainMenu).append(menuCol);
        });

        // If the applet has a menuSearch defined, display the box
        if (newAppletObj.menuSearch) {
            let menuCol = document.createElement("li");
            menuCol.className = "searchBox";
            let searchTag = document.createElement("div");
            $(searchTag).html("&nbsp;&nbsp;&nbsp;&nbsp;Search&nbsp;");
            let inputBox = document.createElement("input");
            newAppletObj.menuSearch.searchField = inputBox;
            $(menuCol).append(searchTag);
            $(menuCol).append(inputBox);
            $(mainMenu).append(menuCol);
        }

        // If the applet has a menuQuery defined, display the box
        if (newAppletObj.menuQuery) {
            let menuCol = document.createElement("li");
            menuCol.className = "queryBox";
            let queryTag = document.createElement("div");
            $(queryTag).html("&nbsp;&nbsp;&nbsp;&nbsp;Query&nbsp;");
            let inputBox = document.createElement("textarea");
            newAppletObj.menuQuery.queryField = inputBox;
            $(menuCol).append(queryTag);
            $(menuCol).append(inputBox);
            $(mainMenu).append(menuCol);
        }

        // Add the menu
        $(newAppletObj.windowDiv).children(".vdmWindowMenu").append(mainMenu);

        // Create and populate footer element with Size Report and Resize button
        $(newAppletObj.windowDiv).children(".vdmWindowFooter").append(
            "<div class=\"sizeReport\">" + $(newAppletObj.windowDiv).width() + "," + $(newAppletObj.windowDiv).height() + "</div>" +
            "<div class=\"resize\"><i class=\"fa fa-angle-double-right fa-lg\"></i></div>"
        );

        // Make Window draggable
        $(newAppletObj.windowDiv).draggable({
            handle: '.vdmWindowHeader',
            containment: [0, thisVDMDesktop.vdmTopBarHeight, $(window).width() - 50, $(window).height() - 50],
            zIndex: 1,
            start: function (event, ui) {
                $(this).css("z-index-cur", $(this).css("z-index"));
                $(this).css("z-index", "999999");
                $(this).css("cursor", "pointer");
            },
            stop: function (event, ui) {
                $(this).css("z-index", $(this).css("z-index-cur"));
                $(this).css("cursor", "auto");
                if (typeof newAppletObj.resizeMovingHook !== "undefined") {
                    newAppletObj.resizeMovingHook();
                }
            }
        });

        // Make Window active on mouse down (if not already selected)
        $(newAppletObj.windowDiv).bind("mousedown", function (e) {
            if (this !== thisVDMDesktop.currentWindowDiv) {
                thisVDMDesktop.switchActiveWindow(this);
            }
        });

        // Make Window active now
        thisVDMDesktop.switchActiveWindow(newAppletObj.windowDiv, true);

        // Apply controls to Resize button
        let divResize = $(newAppletObj.windowDiv).find('.resize');
        $(divResize).mousedown(function (e) {
            let resizeDiv = $(this).parent().parent();
            let containerDiv = $(resizeDiv).parent().parent();
            let divSizeOut = $(resizeDiv).find('.sizeReport');
            let mouseStartX = e.pageX;
            let mouseStartY = e.pageY;
            let pageStartX = $(resizeDiv).width();
            let pageStartY = $(resizeDiv).height();
            $(containerDiv).bind('mousemove', function (e) {
                let gParentDiv = resizeDiv;
                let gParentWidth = pageStartX - (mouseStartX - e.pageX);
                let gParentHeight = pageStartY - (mouseStartY - e.pageY);
                $(gParentDiv).width(gParentWidth);
                $(gParentDiv).height(gParentHeight);
                $(divSizeOut).html(gParentWidth + ',' + gParentHeight);
                if (typeof newAppletObj.resizeMovingHook !== "undefined") {
                    newAppletObj.resizeMovingHook();
                }
            });
            $(containerDiv).bind('mouseup', function (e) {
                $(this).unbind('mousemove');
            });
        });

        // Run post open handler
        if (newAppletObj.postOpenHandler) {
            newAppletObj.postOpenHandler();
        }

        // Run startup script
        if (newAppletObj.runStartup) {
            newAppletObj.runStartup();
        }

        this.vdmWindows.push(newAppletObj);
    }

    closeWindow(closeWindow) {
        let thisVDMDesktop = this;

        // Run Pre Close Handler if it exists
        if (typeof closeWindow.preCloseHandler !== "undefined" && typeof closeWindow.preCloseHandler === 'function') {
            closeWindow.preCloseHandler();
        }

        // Delete Window Element
        let element = document.getElementById(closeWindow.windowID);
        element.parentNode.removeChild(element);

        // Run Post Close
        if (typeof closeWindow.postCloseHandler !== "undefined" && typeof closeWindow.postCloseHandler === 'function') {
            closeWindow.postCloseHandler();
        }

        // Set Windows array instance to null
        let windowIndex = thisVDMDesktop.vdmWindows.indexOf(closeWindow);
        thisVDMDesktop.vdmWindows.splice(windowIndex, 1);
    }

    switchActiveWindow(newActiveDiv) {
        let thisVDMDesktop = this;

        // Make previous window gray (if applicable)
        if (thisVDMDesktop.currentWindowDiv) {
            $(thisVDMDesktop.currentWindowDiv).children('.vdmWindowHeader').first().css('background-color', '#646474');
        }

        // Make this window blue
        thisVDMDesktop.currentWindowDiv = newActiveDiv;
        $(thisVDMDesktop.currentWindowDiv).children('.vdmWindowHeader').first().css('background-color', '#4F6CB8');

        // Make sure we have the higest z-index
        let currentZ = parseFloat($(newActiveDiv).css("z-index"));
        $(".vdmWindow").each(function () {
            if (this !== newActiveDiv) {
                let checkDiv = this;
                let checkZ = parseFloat($(checkDiv).css("z-index"));
                if (checkZ > currentZ) {
                    // Swap with this one
                    $(checkDiv).css("z-index", currentZ);
                    $(newActiveDiv).css("z-index", checkZ);
                    currentZ = checkZ;
                } else if (checkZ === currentZ) {
                    // Set to 1 higher
                    currentZ++;
                    $(newActiveDiv).css("z-index", currentZ);
                }
            }
        });
    }
}

/**
 * Base Window attributes
 * @param {string} title Window title
 * @param {number} sizeX Window width
 * @param {number} sizeY Window height
 */
class BaseWindowDef {
    constructor(title, sizeX, sizeY) {
        this.title = title;
        this.sizeX = sizeX;
        this.sizeY = sizeY;
    }
}

/**
 * VDM Desktop manager
 * @param {string} appletName Applet Name
 * @param {string} appletIcon Applet Icon
 */
class VDMAppletProfile {
    constructor() {
        this.appletName = "";
        this.appletIcon = "";
        this.appletPath = "";
        this.appletScript = "";
        this.appletClass = null;
        this.showInMenu = true;
        this.startupScript = "";
        this.title = "";
        this.sizeX = 300;
        this.sizeY = 250;
    }
}

/**
 * VDM Window
 * @param {BaseWindowDef} windowProfile Profile describing new Window
 */
class VDMWindow {
    constructor(windowProfile) {

        // Assigned on newWindow
        this.vdmDesktop = null;

        // Attributes from profile
        this.title = windowProfile.title;
        this.sizeX = windowProfile.sizeX;
        this.sizeY = windowProfile.sizeY;

        // Attributes which will be assigned upon window DIV creation
        this.windowID = null;
        this.windowDiv = null;
        this.windowParts = {
            "header": null,
            "menu": null,
            "data": null,
            "footer": null,
            "popover": null
        };

        // Attributes which should be specified in each applet definition
        this.preReqs = [];

        this.menu = {};

        this.menuSearch = null;

        this.menuQuery = null;

        this.appFuncs = {};

        this.appVars = {};
    }

    runStartup() {
    }

    /**
     * Split paneDiv into left and right panes
     * @param {HTMLDivElement} paneDiv DIV to split
     * @param {number} splitOffset Offset from left
     * @param {boolean} scrollLeft Offer scroll on returned left pane
     * @param {boolean} scrollRight Offer scroll on returned right pane
     * @return {HTMLDivElement[]} Array of return elements [leftPane, divider, rightPane]
     */
    splitPaneHorizontal(paneDiv, splitOffset, scrollLeft, scrollRight) {
        let thisVDMWindow = this;
        $(paneDiv).addClass("parent");
        let a = document.createElement("div");
        a.className = "dwData dwData-LeftPane";
        let b = document.createElement("div");
        b.className = "dwData dwData-VDiv";
        let c = document.createElement("div");
        c.className = "dwData dwData-RightPane";
        $(paneDiv).append(a);
        $(paneDiv).append(b);
        $(paneDiv).append(c);
        $(b).css({ left: splitOffset + 'px' });
        $(a).css({ width: splitOffset - 1 + 'px' });
        $(c).css({ left: splitOffset + 4 + 'px' });

        if (scrollLeft) { $(a).css({ "overflow-y": "auto" }); } else { $(a).css({ "overflow-y": "hidden" }); }
        if (scrollRight) { $(c).css({ "overflow-y": "auto" }); } else { $(c).css({ "overflow-y": "hidden" }); }

        $(b).mousedown(function (e) {
            let mouseStartX = e.pageX;
            let pageStartX = parseInt($(b).css('left'));
            $(paneDiv).bind('mousemove', function (e) {
                let newOffset = pageStartX + (e.pageX - mouseStartX);
                $(b).css({ left: newOffset + 'px' });
                $(a).css({ width: newOffset - 1 + 'px' });
                $(c).css({ left: newOffset + 4 + 'px' });
            });
            $(paneDiv).bind('mouseup', function (e) {
                $(this).unbind('mousemove');
                if (typeof thisVDMWindow.resizeMovingHook !== "undefined") {
                    thisVDMWindow.resizeMovingHook();
                }
            });
        });
        return [a, b, c];
    }

    /**
     * Split paneDiv into top and bottom panes
     * @param {HTMLDivElement} paneDiv DIV to split
     * @param {number} splitOffset Offset from top
     * @param {boolean} scrollTop Offer scroll on returned top pane
     * @param {boolean} scrollBottom Offer scroll on returned bottom pane
     * @return {HTMLDivElement[]} Array of return elements [topPane, divider, bottomPane]
     */
    splitPaneVertical(paneDiv, splitOffset, scrollTop, scrollBottom) {
        let thisVDMWindow = this;
        $(paneDiv).addClass("parent");
        let a = document.createElement("div");
        a.className = "dwData dwData-TopPane";
        let b = document.createElement("div");
        b.className = "dwData dwData-HDiv";
        let c = document.createElement("div");
        c.className = "dwData dwData-BottomPane";
        $(paneDiv).append(a);
        $(paneDiv).append(b);
        $(paneDiv).append(c);
        $(b).css({ top: splitOffset + 'px' });
        $(a).css({ height: splitOffset - 1 + 'px' });
        $(c).css({ top: splitOffset + 4 + 'px' });

        if (scrollTop) { $(a).css({ "overflow-y": "auto" }); } else { $(a).css({ "overflow-y": "hidden" }); }
        if (scrollBottom) { $(c).css({ "overflow-y": "auto" }); } else { $(c).css({ "overflow-y": "hidden" }); }

        $(b).mousedown(function (e) {
            let mouseStartY = e.pageY;
            let pageStartY = parseInt($(b).css('top'));
            $(paneDiv).bind('mousemove', function (e) {
                let newOffset = pageStartY + (e.pageY - mouseStartY);
                $(b).css({ top: newOffset + 'px' });
                $(a).css({ height: newOffset - 1 + 'px' });
                $(c).css({ top: newOffset + 4 + 'px' });
            });
            $(paneDiv).bind('mouseup', function (e) {
                $(this).unbind('mousemove');
                if (typeof thisVDMWindow.resizeMovingHook !== "undefined") {
                    thisVDMWindow.resizeMovingHook();
                }
            });
        });
        return [a, b, c];
    }
}

class VDMApplet extends VDMWindow {
    constructor(appletProfile) {
        super(appletProfile);

        let thisVDMApplet = this;

        // Attributes from profile
        this.appletName = appletProfile.appletName;
        this.appletPath = appletProfile.appletPath;

        // Attributes which will be assigned upon window DIV creation
        this.appletIndex = 0;
    }
}

class VDMCollapseTree {
    constructor(menuParentDiv) {
        //let thisTreeMenu = this;
        this.parentDiv = menuParentDiv;
        this.parentDiv.innerHTML = '';
        this.menuTopUL = document.createElement("ul");
        this.menuTopUL.className = "vdm-CollapseTree";
        this.parentDiv.appendChild(this.menuTopUL);
    }

    addItem(parentObjRef, newItemTag, newItemClass, newItemRef, isParent, clickFunction) {
        // First use subMenuArr to find the target UL
        let targetUL = null;
        if (parentObjRef) {
            targetUL = parentObjRef.leftMenuUL;
        } else {
            targetUL = this.menuTopUL;
        }

        let newLI = document.createElement("li");
        let newSpan = document.createElement("span");
        newSpan.className = newItemClass;
        newSpan.innerHTML = newItemTag;
        newItemRef.leftMenuSpan = newSpan;
        newItemRef.leftMenuLI = newLI;
        newLI.appendChild(newSpan);
        if (isParent) {
            newLI.className = "parent";
            let newUL = document.createElement("ul");
            newLI.appendChild(newUL);
            newItemRef.leftMenuUL = newUL;
            $(newSpan).on('click', function () {
                $(this).parent().toggleClass('active');
                $(this).parent().children('ul').toggle();
                clickFunction(newItemRef);
            });
        } else {
            $(newSpan).on('click', function () {
                clickFunction(newItemRef);
            });
        }
        targetUL.appendChild(newLI);
    }
}