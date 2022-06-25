;(function(){ 'use strict';

  var $ = typeof jQuery === typeof undefined ? null : jQuery;

  var register = function( cytoscape, $ ){
    
    if( !cytoscape ){ return; } // can't register if cytoscape unspecified
    
    var defaults = {
      // List of initial menu items
      menuItems: [
        /*
        {
          id: 'remove',
          content: 'remove',
          tooltipText: 'remove',
          selector: 'node, edge',
          onClickFunction: function () {
            console.log('remove element');
          },
          hasTrailingDivider: true
        },
        {
          id: 'hide',
          content: 'hide',
          tooltipText: 'remove',
          selector: 'node, edge',
          onClickFunction: function () {
            console.log('hide element');
          },
          disabled: true
        }*/
      ],
      // css classes that menu items will have
      menuItemClasses: [
        // add class names to this list
      ],
      // css classes that context menu will have
      contextMenuClasses: [
        // add class names to this list
      ]
    };
    
    var eventCyTapStart; // The event to be binded on tap start
    
    // To initialize with options.
    cytoscape('core', 'contextMenus', function (opts) {
      var cy = this;
      
      // Initilize scratch pad
      if (!cy.scratch('cycontextmenus')) {
        cy.scratch('cycontextmenus', {});
      }
      
      var options = getScratchProp('options');
      var $cxtMenu = getScratchProp('cxtMenu');
      var menuItemCSSClass = 'cy-context-menus-cxt-menuitem';
      var dividerCSSClass = 'cy-context-menus-divider';
      
      // Merge default options with the ones coming from parameter
      function extend(defaults, options) {
        var obj = {};

        for (var i in defaults) {
          obj[i] = defaults[i];
        }

        for (var i in options) {
          obj[i] = options[i];
        }

        return obj;
      };

      function getScratchProp(propname) {
        return cy.scratch('cycontextmenus')[propname];
      };
      
      function setScratchProp(propname, value) {
        cy.scratch('cycontextmenus')[propname] = value;
      };

      function preventDefaultContextTap() {
        $(".cy-context-menus-cxt-menu").contextmenu( function() {
            return false;
        });
      }

      // Get string representation of css classes
      function getMenuItemClassStr(classes, hasTrailingDivider) {
        var str = getClassStr(classes);

        str += ' ' + menuItemCSSClass;

        if(hasTrailingDivider) {
          str += ' ' + dividerCSSClass;
        }

        return str;
      }

      // Get string representation of css classes
      function getClassStr(classes) {
        var str = '';

        for( var i = 0; i < classes.length; i++ ) {
          var className = classes[i];
          str += className;
          if(i !== classes.length - 1) {
            str += ' ';
          }
        }

        return str;
      }

      function displayComponent($component) {
        $component.css('display', 'block');
      }

      function hideComponent($component) {
        $component.css('display', 'none');
      }

      function hideMenuItemComponents() {
        $cxtMenu.children().css('display', 'none');
      }

      function bindOnClickFunction($component, onClickFcn) {
        var callOnClickFcn;

        $component.on('click', callOnClickFcn = function() {
          onClickFcn(getScratchProp('currentCyEvent'));
        });

        $component.data('call-on-click-function', callOnClickFcn); 
      }

      function bindCyCxttap($component, selector, coreAsWell) {
        function _cxtfcn(event) {
          setScratchProp('currentCyEvent', event);
          adjustCxtMenu(event); // adjust the position of context menu
          if ($component.data('show')) {
            // Now we have a visible element display context menu if it is not visible
            if (!$cxtMenu.is(':visible')) {
              displayComponent($cxtMenu);
            }
            // anyVisibleChild indicates if there is any visible child of context menu if not do not show the context menu
            setScratchProp('anyVisibleChild', true);// there is visible child
            displayComponent($component); // display the component
          }

          // If there is no visible element hide the context menu as well(If it is visible)
          if (!getScratchProp('anyVisibleChild') && $cxtMenu.is(':visible')) {
            hideComponent($cxtMenu);
          }
        }

        var cxtfcn;
        var cxtCoreFcn;

        if(coreAsWell) {
          cy.on('cxttap', cxtCoreFcn = function(event) {
            var target = event.target || event.cyTarget;
            if( target != cy ) {
              return;
            }

            _cxtfcn(event);
          });
        }

        if(selector) {
          cy.on('cxttap', selector, cxtfcn = function(event) {
            _cxtfcn(event);
          });
        }

        // Bind the event to menu item to be able to remove it back
        $component.data('cy-context-menus-cxtfcn', cxtfcn);
        $component.data('cy-context-menus-cxtcorefcn', cxtCoreFcn);
      }

      function bindCyEvents() {
        cy.on('tapstart', eventCyTapStart = function(){
          hideComponent($cxtMenu);
          setScratchProp('cxtMenuPosition', undefined);
          setScratchProp('currentCyEvent', undefined);
        });
      }

      function performBindings($component, onClickFcn, selector, coreAsWell) {
        bindOnClickFunction($component, onClickFcn);
        bindCyCxttap($component, selector, coreAsWell);
      }

      // Adjusts context menu if necessary
      function adjustCxtMenu(event) {
        var currentCxtMenuPosition = getScratchProp('cxtMenuPosition');
        var cyPos = event.position || event.cyPosition;

        if( currentCxtMenuPosition != cyPos ) {
          hideMenuItemComponents();
          setScratchProp('anyVisibleChild', false);// we hide all children there is no visible child remaining
          setScratchProp('cxtMenuPosition', cyPos);

          var containerPos = $(cy.container()).offset();
          var renderedPos = event.renderedPosition || event.cyRenderedPosition;

          var left = containerPos.left + renderedPos.x;
          var top = containerPos.top + renderedPos.y;

          $cxtMenu.css('left', left);
          $cxtMenu.css('top', top);
        }
      }

      function createAndAppendMenuItemComponents(menuItems) {
        for (var i = 0; i < menuItems.length; i++) {
          createAndAppendMenuItemComponent(menuItems[i]);
        }
      }

      function createAndAppendMenuItemComponent(menuItem) {
        // Create and append menu item
        var $menuItemComponent = createMenuItemComponent(menuItem);
        appendComponentToCxtMenu($menuItemComponent);

        performBindings($menuItemComponent, menuItem.onClickFunction, menuItem.selector, menuItem.coreAsWell);
      }//insertComponentBeforeExistingItem(component, existingItemID)

      function createAndInsertMenuItemComponentBeforeExistingComponent(menuItem, existingComponentID) {
        // Create and insert menu item
        var $menuItemComponent = createMenuItemComponent(menuItem);
        insertComponentBeforeExistingItem($menuItemComponent, existingComponentID);

        performBindings($menuItemComponent, menuItem.onClickFunction, menuItem.selector, menuItem.coreAsWell);
      }

      // create cxtMenu and append it to body
      function createAndAppendCxtMenuComponent() {
        var classes = getClassStr(options.contextMenuClasses);
//        classes += ' cy-context-menus-cxt-menu';
        $cxtMenu = $('<div class=' + classes + '></div>');
        $cxtMenu.addClass('cy-context-menus-cxt-menu');
        setScratchProp('cxtMenu', $cxtMenu);

        $('body').append($cxtMenu);
        return $cxtMenu;
      }

      // Creates a menu item as an html component
      function createMenuItemComponent(item) {
        var classStr = getMenuItemClassStr(options.menuItemClasses, item.hasTrailingDivider);
        var itemStr = '<button id="' + item.id + '" class="' + classStr + '"';

        if(item.tooltipText) {
          itemStr += ' title="' + item.tooltipText + '"';
        }

        if(item.disabled) {
          itemStr += ' disabled';
        }
        if (!item.image){
            itemStr += '>' + item.content + '</button>';
        }
        else{
            itemStr += '>' + '<img src="' + item.image.src + '" width="' + item.image.width + 'px"; height="'
                + item.image.height + 'px"; style="position:absolute; top: ' + item.image.y + 'px; left: '
                + item.image.x + 'px;">' + item.content + '</button>';
        };

        var $menuItemComponent = $(itemStr);

        $menuItemComponent.data('selector', item.selector); 
        $menuItemComponent.data('on-click-function', item.onClickFunction);
        $menuItemComponent.data('show', (typeof(item.show) === 'undefined' || item.show));  
        return $menuItemComponent;
      }

      // Appends the given component to cxtMenu
      function appendComponentToCxtMenu(component) {
        $cxtMenu.append(component);
        bindMenuItemClickFunction(component);
      }

      // Insert the given component to cxtMenu just before the existing item with given ID
      function insertComponentBeforeExistingItem(component, existingItemID) {
        var $existingItem = $('#' + existingItemID);
        component.insertBefore($existingItem);
      }

      function destroyCxtMenu() {
        if(!getScratchProp('active')) {
          return;
        }

        removeAndUnbindMenuItems();

        cy.off('tapstart', eventCyTapStart);

        $cxtMenu.remove();
        $cxtMenu = undefined;
        setScratchProp($cxtMenu, undefined);
        setScratchProp('active', false);
        setScratchProp('anyVisibleChild', false);
      }

      function removeAndUnbindMenuItems() {
        var children = $cxtMenu.children();

        $(children).each(function() {
          removeAndUnbindMenuItem($(this));
        });
      }

      function removeAndUnbindMenuItem(itemID) {
        var $component = typeof itemID === 'string' ? $('#' + itemID) : itemID;
        var cxtfcn = $component.data('cy-context-menus-cxtfcn');
        var selector = $component.data('selector');
        var callOnClickFcn = $component.data('call-on-click-function');
        var cxtCoreFcn = $component.data('cy-context-menus-cxtcorefcn');

        if(cxtfcn) {
          cy.off('cxttap', selector, cxtfcn);
        }

        if(cxtCoreFcn) {
          cy.off('cxttap', cxtCoreFcn);
        }

        if(callOnClickFcn) {
          $component.off('click', callOnClickFcn);
        }

        $component.remove();
      }

      function moveBeforeOtherMenuItemComponent(componentID, existingComponentID) {
        if( componentID === existingComponentID ) {
          return;
        }

        var $component = $('#' + componentID).detach();
        var $existingComponent = $('#' + existingComponentID);

        $component.insertBefore($existingComponent);
      }

      function bindMenuItemClickFunction(component) {
        component.click( function() {
            hideComponent($cxtMenu);
            setScratchProp('cxtMenuPosition', undefined);
        });
      }

      function disableComponent(componentID) {
        $('#' + componentID).attr('disabled', true);
      }

      function enableComponent(componentID) {
        $('#' + componentID).attr('disabled', false);
      }

      function setTrailingDivider(componentID, status) {
        var $component = $('#' + componentID);
        if(status) {
          $component.addClass(dividerCSSClass);
        }
        else {
          $component.removeClass(dividerCSSClass);
        }
      }

      // Get an extension instance to enable users to access extension methods
      function getInstance(cy) {
        var instance = {
          // Returns whether the extension is active
         isActive: function() {
           return getScratchProp('active');
         },
         // Appends given menu item to the menu items list.
         appendMenuItem: function(item) {
           createAndAppendMenuItemComponent(item);
           return cy;
         },
         // Appends menu items in the given list to the menu items list.
         appendMenuItems: function(items) {
           createAndAppendMenuItemComponents(items);
           return cy;
         },
         // Removes the menu item with given ID.
         removeMenuItem: function(itemID) {
           removeAndUnbindMenuItem(itemID);
           return cy;
         },
         // Sets whether the menuItem with given ID will have a following divider.
         setTrailingDivider: function(itemID, status) {
           setTrailingDivider(itemID, status);
           return cy;
         },
         // Inserts given item before the existingitem.
         insertBeforeMenuItem: function(item, existingItemID) {
           createAndInsertMenuItemComponentBeforeExistingComponent(item, existingItemID);
           return cy;
         },
         // Moves the item with given ID before the existingitem.
         moveBeforeOtherMenuItem: function(itemID, existingItemID) {
           moveBeforeOtherMenuItemComponent(itemID, existingItemID);
           return cy;
         },
         // Disables the menu item with given ID.
         disableMenuItem: function(itemID) {
           disableComponent(itemID);
           return cy;
         },
         // Enables the menu item with given ID.
         enableMenuItem: function(itemID) {
           enableComponent(itemID);
           return cy;
         },
         // Disables the menu item with given ID.
         hideMenuItem: function(itemID) {
           $('#'+itemID).data('show', false);
           hideComponent($('#'+itemID));
           return cy;
         },
         // Enables the menu item with given ID.
         showMenuItem: function(itemID) {
           $('#'+itemID).data('show', true);
           displayComponent($('#'+itemID));
           return cy;
         },
         // Destroys the extension instance
         destroy: function() {
           destroyCxtMenu();
           return cy;
         }
        };

        return instance;
      }

      if ( opts !== 'get' ) {
        // merge the options with default ones
        options = extend(defaults, opts);
        setScratchProp('options', options);

        // Clear old context menu if needed
        if(getScratchProp('active')) {
          destroyCxtMenu();
        }

        setScratchProp('active', true);

        $cxtMenu = createAndAppendCxtMenuComponent();

        var menuItems = options.menuItems;
        createAndAppendMenuItemComponents(menuItems);

        bindCyEvents();
        preventDefaultContextTap();
      }
      
      return getInstance(this);
    });
  };

  if( typeof module !== 'undefined' && module.exports ){ // expose as a commonjs module
    module.exports = register;
  }

  if( typeof define !== 'undefined' && define.amd ){ // expose as an amd/requirejs module
    define('cytoscape-context-menus', function(){
      return register;
    });
  }

  if( typeof cytoscape !== 'undefined' && $ ){ // expose to global cytoscape (i.e. window.cytoscape)
    register( cytoscape, $ );
  }

})();
