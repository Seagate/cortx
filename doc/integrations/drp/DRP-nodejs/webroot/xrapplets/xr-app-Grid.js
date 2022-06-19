(class extends XRApplet {
    constructor(appletProfile, xrSession) {
        super(appletProfile, xrSession);
        let myApp = this;
    }

    async runStartup() {
        let myApp = this;
        let engine = myApp.xrSession.babylonEngine;

        let multimediaRoot = ".";


        let canvas = document.getElementById("renderCanvas");
        let skyMaterial;
        var sphere;
        let globe;
        let controllerHash = {
            aPressed: false,
            bPressed: false,
            xPressed: false,
            yPressed: false,

            onAChange: function (stateObject) {
                if (!controllerHash.aPressed) {
                    if (stateObject.pressed) {
                        controllerHash.aPressed = true;
                        console.log("Pressed A")
                    }
                } else {
                    if (!stateObject.pressed) {
                        controllerHash.aPressed = false;
                        console.log("Released A")
                    }
                }
            },

            onBChange: function (stateObject) {
                if (!controllerHash.bPressed) {
                    if (stateObject.pressed) {
                        controllerHash.bPressed = true;
                        console.log("Pressed B")
                    }
                } else {
                    if (!stateObject.pressed) {
                        controllerHash.bPressed = false;
                        console.log("Released B")
                    }
                }
            },

            onXChange: function (stateObject) {
                if (!controllerHash.xPressed) {
                    if (stateObject.pressed) {
                        controllerHash.xPressed = true;
                        console.log("Pressed X")
                    }
                } else {
                    if (!stateObject.pressed) {
                        controllerHash.xPressed = false;
                        console.log("Released X")
                    }
                }
            },

            onYChange: function (stateObject) {
                if (!controllerHash.yPressed) {
                    if (stateObject.pressed) {
                        controllerHash.yPressed = true;
                        console.log("Pressed Y")
                    }
                } else {
                    if (!stateObject.pressed) {
                        controllerHash.yPressed = false;
                        console.log("Released Y")
                    }
                }
            }
        };

        // This creates a basic Babylon Scene object (non-mesh)

        let scene = new BABYLON.Scene(engine);
        let xr = await scene.createDefaultXRExperienceAsync();

        skyMaterial = new BABYLON.StandardMaterial("skybox", scene);
        skyMaterial.backFaceCulling = false;
        skyMaterial.reflectionTexture = new BABYLON.CubeTexture("assets/webxr/textures/skybox02", scene, ["_px.png", "_py.png", "_pz.png", "_ax.png", "_ay.png", "_az.png"]);
        skyMaterial.reflectionTexture.coordinatesMode = BABYLON.Texture.SKYBOX_MODE;
        //skyMaterial.reflectionTexture.coordinatesMode = BABYLON.Texture.CUBIC_MODE;
        skyMaterial.diffuseColor = new BABYLON.Color3(0, 0, 0);
        skyMaterial.specularColor = new BABYLON.Color3(0, 0, 0);
        //let skybox = BABYLON.Mesh.CreateBox("skybox", 10.0, scene);
        let skybox = BABYLON.MeshBuilder.CreateBox("skyBox", { size: 10.0 }, scene);
        skybox.material = skyMaterial;
        skybox.position.set(0, 1, 0);

        let camera = new BABYLON.FreeCamera("", new BABYLON.Vector3(0, 0, -1), scene)
        let light = new BABYLON.DirectionalLight("", new BABYLON.Vector3(0, -1, 1), scene)
        light.specular = new BABYLON.Color3(0, 0, 0);
        light.diffuse = new BABYLON.Color3(1, 1, 1);
        light.intensity = .4;
        light.position.y = 10;

        sphere = BABYLON.Mesh.CreateSphere("sphere", 100, .5, scene);
        sphere.rotation.x = Math.PI;
        sphere.position.set(0, 1.5, 2); // lateral, height, depth

        let alpha = 0;
        scene.beforeRender = function () {
            sphere.rotation.y = alpha;
            alpha -= 0.01;
        };

        //Add material to sphere
        let groundMaterial = new BABYLON.StandardMaterial("mat", scene);
        groundMaterial.emissiveTexture = new BABYLON.Texture('assets/webxr/textures/earth2-normal.jpg', scene);
        groundMaterial.bumpTexture = new BABYLON.Texture('assets/webxr/textures/earthbump.jpg', scene);
        //groundMaterial.diffuseTexture.uRotationCenter = 0;
        //groundMaterial.diffuseTexture.vRotationCenter = 0;
        //groundMaterial.diffuseTexture.wRotationCenter = 0;
        groundMaterial.emissiveTexture.level = .6;
        sphere.material = groundMaterial;

        //let ground = BABYLON.Mesh.CreateGround("ground1", 6, 6, 2, scene);

        //let leftHand = BABYLON.Mesh.CreateBox("",0.1, scene)
        //leftHand.scaling.z = 2;
        //let rightHand = leftHand.clone()
        let head = BABYLON.Mesh.CreateBox("", 0.2, scene)
        head.position.set(-0.5, 1, 0);
        console.log("hookAdded = false");

        let hookAdded = false;

        let counter = 0;
        let controllersDetected = false;
        //if (!vrHelper.webVRCamera) return null;
        //console.dir(vrHelper.webVRCamera.controllers);

        let music = new BABYLON.Sound("Music", "assets/webxr/sounds/jg-032316-sfx-sub-pulse.mp3", scene, null, { loop: true, autoplay: true });

        // Logic to be run every frame
        scene.onBeforeRenderObservable.add(() => {
            /*
            if ((! vrHelper.webVRCamera.leftController) && vrHelper.webVRCamera.controllers[0]) {
                vrHelper.webVRCamera.leftController = vrHelper.webVRCamera.controllers[0];
                vrHelper.webVRCamera.rightController = vrHelper.webVRCamera.controllers[1];
                console.log("Added controller objects");
            }
            */
            // Left and right hand position/rotation
            /*
            if(vrHelper._leftController){
                leftHand.position = vrHelper._leftController.webVRController.devicePosition.clone()
                leftHand.rotationQuaternion = vrHelper._leftController.webVRController.deviceRotationQuaternion.clone()
            }
            if(vrHelper._rightController){
    
                if (! hookAdded) {
                    console.log("hookAdded = true");
                    hookAdded = true;
                    console.log("Hook added");
                    vrHelper._rightController.webVRController.onAButtonStateChangedObservable.add(()=>{
                        console.log("Button state changed");
                    })
                }
    
                rightHand.position = vrHelper._rightController.webVRController.devicePosition.clone()
                rightHand.rotationQuaternion = vrHelper._rightController.webVRController.deviceRotationQuaternion.clone()
            }
            */
            // Head position/rotation
            //head.position = vrHelper.webVRCamera.devicePosition.clone()
            //head.rotationQuaternion = vrHelper.webVRCamera.deviceRotationQuaternion.clone()
            //head.rotationQuaternion.z = 0;
            //head.position.z = 2;

            if (0 && counter === 360) {
                if (!controllersDetected && vrHelper.webVRCamera.controllers.length > 0) {
                    controllersDetected = true;
                    console.dir(vrHelper.webVRCamera);
                    console.dir(vrHelper.webVRCamera.controllers);

                    // Button Hooks
                    vrHelper.webVRCamera.rightController.onAButtonStateChangedObservable.add(controllerHash.onAChange);
                    vrHelper.webVRCamera.rightController.onBButtonStateChangedObservable.add(controllerHash.onBChange);
                    vrHelper.webVRCamera.leftController.onXButtonStateChangedObservable.add(controllerHash.onXChange);
                    vrHelper.webVRCamera.leftController.onYButtonStateChangedObservable.add(controllerHash.onYChange);

                }
                counter = 0;
            }

            counter++;
        })

        myApp.BabylonScene = scene;
        myApp.xr = xr;

        engine.runRenderLoop(function () {
            scene.render();
            for (let i = 0; i < xr.input.controllers.length; i++) {
                if (!xr.input.controllers[i].motionController) continue;
                let gamepad = xr.input.controllers[i].motionController.gamepadObject;
                if ("hapticActuators" in gamepad && gamepad.hapticActuators.length > 0) {
                    for (let j = 0; j < gamepad.buttons.length; ++j) {
                        if (gamepad.buttons[j].pressed) {
                            // Vibrate the gamepad using to the value of the button as
                            // the vibration intensity, normalized to 0.0..1.0 range.
                            gamepad.hapticActuators[0].pulse((j + 1.0) / (gamepad.buttons.length + 1), 1000);
                            break;
                        }
                    }
                }
            }
        });

        /*
        BABYLON.SceneLoader.Load("", `${multimediaRoot}/testscene.babylon`, engine, function (newScene) {
            // Wait for textures and shaders to be ready
            newScene.executeWhenReady(async function () {

                let musicFile = `${multimediaRoot}/mixkit-forest-ambience-loop-1228.mp3`;

                // Attach camera to canvas inputs
                //newScene.activeCamera.attachControl(canvas);
                let ground = newScene.getMeshByID("Plane");
                let tree1 = newScene.getMeshByID("Tree1");
                let tree2 = newScene.getMeshByID("Tree2");

                // here we add XR support
                const xr = await newScene.createDefaultXRExperienceAsync({
                    floorMeshes: [ground],
                    //disableDefaultUI: true,
                    //disableTeleportation: true
                });
                //xr.teleportation.teleportationEnabled = false;
                for (let i = 0; i < newScene.lights.length; i++) {
                    newScene.lights[i].intensity = .75;
                }

                let getRandomRotation = () => {
                    let randomAngle = Math.floor(Math.random() * 360);
                    let radians = BABYLON.Tools.ToRadians(randomAngle);
                    return radians;
                }

                let rows = 10;
                let cols = 10;
                for (let i = 0; i < rows; i++) {
                    for (let j = 0; j < cols; j++) {
                        let newTree = tree1.clone();
                        newTree.rotation.y = getRandomRotation();
                        newTree.position.x += i * 15 - (rows * 15 / 2);
                        newTree.position.z += j * 15;
                    }
                }

                let music = new BABYLON.Sound("Music", musicFile, newScene, null, { loop: true, autoplay: true });

                let newMeshes = await BABYLON.SceneLoader.ImportMeshAsync(null, `${multimediaRoot}/hazel/`, "Hazel_01.gltf", newScene);

                let mouse1 = newMeshes.transformNodes[0];
                mouse1.scaling.scaleInPlace(0.05);
                mouse1.rotate(BABYLON.Axis.Y, BABYLON.Tools.ToRadians(180));
                mouse1.position.z -= 10;

                let mouse2 = mouse1.clone();
                mouse2.position.x += 4;
                mouse2.rotation.y = BABYLON.Tools.ToRadians(180);

                mouse1.position.x += 4;

                let newMeshes2 = await BABYLON.SceneLoader.ImportMeshAsync(null, `${multimediaRoot}/cow/`, "cow.glb", newScene);
                let newCow = newMeshes2.meshes[0];
                newCow.scaling.scaleInPlace(0.25);
                newCow.position.z -= 10;
                //newCow.rotationQuaternion =
                newCow.rotate(BABYLON.Axis.Y, BABYLON.Tools.ToRadians(180));

                myApp.BabylonScene = newScene;
                myApp.xr = xr;

                engine.runRenderLoop(function () {
                    newScene.render();
                    for (let i = 0; i < xr.input.controllers.length; i++) {
                        if (!xr.input.controllers[i].motionController) continue;
                        let gamepad = xr.input.controllers[i].motionController.gamepadObject;
                        if ("hapticActuators" in gamepad && gamepad.hapticActuators.length > 0) {
                            for (let j = 0; j < gamepad.buttons.length; ++j) {
                                if (gamepad.buttons[j].pressed) {
                                    // Vibrate the gamepad using to the value of the button as
                                    // the vibration intensity, normalized to 0.0..1.0 range.
                                    gamepad.hapticActuators[0].pulse((j + 1.0) / (gamepad.buttons.length + 1), 1000);
                                    break;
                                }
                            }
                        }
                    }
                });

            });
        }, function (progress) {
            // To do: give progress feedback to user
        });
        */

    }
});
//# sourceURL=xr-app-Grid.js
