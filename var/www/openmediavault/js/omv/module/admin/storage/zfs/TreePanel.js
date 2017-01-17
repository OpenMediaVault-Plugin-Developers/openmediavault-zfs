/**
 * This file is part of OpenMediaVault.
 *
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author    Volker Theile <volker.theile@openmediavault.org>
 * @copyright Copyright (c) 2009-2014 Volker Theile
 *
 * OpenMediaVault is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * OpenMediaVault is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenMediaVault. If not, see <http://www.gnu.org/licenses/>.
 */
// require("js/omv/tree/Panel.js")
// require("js/omv/grid/Panel.js")
// require("js/omv/grid/column/BinaryUnit.js")
// require("js/omv/grid/column/BooleanIcon.js")
// require("js/omv/grid/column/BooleanText.js")
// require("js/omv/grid/column/Empty.js")
// require("js/omv/grid/column/Hyperlink.js")
// require("js/omv/grid/column/UnixTimestamp.js")
// require("js/omv/grid/column/WhiteSpace.js")
// require("js/omv/window/MessageBox.js")

/**
 * @ingroup webgui
 * @class OMV.workspace.grid.Panel
 * @derived OMV.grid.Panel
 * An enhanced grid panel. This grid provides 'Add', 'Edit' and 'Delete'
 * buttons in the toolbar by default. The basic delete functionality is also
 * implemented, simply overwrite the 'doDeletion' and 'afterDeletion'
 * functions to implement fit your requirements. To implement the 'Add' and
 * 'Edit' functionality overwrite the 'onAdd' and 'onEdit' callback
 * functions. A paging toolbar which is displayed at the bottom of the grid
 * can be displayed also. It is also possible to reload the grid
 * automatically in a given interval.
 * @param hideTopToolbar TRUE to hide the whole toolbar. Defaults to FALSE.
 * @param hidePagingToolbar TRUE to hide the paging toolbar at the bottom of
 *   the grid. Defaults to TRUE.
 * @param hideAddButton Hide the 'Add' button in the top toolbar.
 *   Defaults to FALSE.
 * @param hideEditButton Hide the 'Edit' button in the top toolbar.
 *   Defaults to FALSE.
 * @param hideDeleteButton Hide the 'Delete' button in the top toolbar.
 *   Defaults to FALSE.
 * @param hideUpButton Hide the 'Up' button in the top toolbar.
 *   Defaults to TRUE.
 * @param hideDownButton Hide the 'Down' button in the top toolbar.
 *   Defaults to TRUE.
 * @param hideApplyButton Hide the 'Apply' button in the top toolbar.
 *   Defaults to TRUE.
 * @param hideRefreshButton Hide the 'Refresh' button in the top toolbar.
 *   Defaults to TRUE.
 * @param addButtonText The button text. Defaults to 'Add'.
 * @param editButtonText The button text. Defaults to 'Edit'.
 * @param deleteButtonText The button text. Defaults to 'Delete'.
 * @param upButtonText The button text. Defaults to 'Up'.
 * @param downButtonText The button text. Defaults to 'Down'.
 * @param applyButtonText The button text. Defaults to 'Save'.
 * @param refreshButtonText The button text. Defaults to 'Refresh'.
 * @param deletionConfirmRequired Set to TRUE to force the user to confirm
 *   the deletion request. Defaults to TRUE.
 * @param deletionWaitMsg The message displayed during the deletion process.
 * @param mode The mode how to retrieve the data displayed in the grid panel.
 *   This can be 'local' or 'remote' which means the data is requested via
 *   RPC. Defaults to 'remote'.
 * @param rememberSelected TRUE to reselect the previous selected rows
 *   after the grid content has been reloaded/refreshed. Defaults to FALSE.
 */
Ext.define("OMV.module.admin.storage.zfs.TreePanel", {
	extend: "OMV.tree.Panel",
	requires: [
		"OMV.window.MessageBox",
		"OMV.grid.column.BinaryUnit",
		"OMV.grid.column.BooleanIcon",
		"OMV.grid.column.BooleanText",
		"OMV.grid.column.Empty",
		"OMV.grid.column.Hyperlink",
		"OMV.grid.column.UnixTimestamp",
		"OMV.grid.column.WhiteSpace"
	],

	border: false,
	rowLines: false,
	columnLines: true,
	selModel: {
		allowDeselect: true,
		mode: "SINGLE"
	},

	hideTopToolbar: false,
	hidePagingToolbar: true,
	hideAddButton: false,
	hideAddObjButton: true,
	hideEditButton: true,
	hideDeleteButton: true,
	hideUpButton: true,
	hideDownButton: true,
	hideApplyButton: true,
	hideRefreshButton: true,
	hideExpandPoolButton: true,
	hideImportPoolButton: false,
	hideScrubButton: true,
	hideExportPoolButton: true,
	hideRenameButton: true,
	addButtonText: _("Add Pool"),
	addObjButtonText: _("Add Object"),
	settingsButtonText: _("Settings"),
	expandPoolButtonText: _("Expand"),
	scrubButtonText: _("Scrub"),
	importPoolButtonText: _("Import Pool"),
	exportPoolButtonText: _("Export Pool"),
	renameButtonText: _("Rename"),
	editButtonText: _("Edit"),
	deleteButtonText: _("Delete"),
	rollbackButtonText: _("Rollback"),
	upButtonText: _("Up"),
	downButtonText: _("Down"),
	applyButtonText: _("Save"),
	refreshButtonText: _("Refresh"),
	deletionConfirmRequired: true,
	deletionWaitMsg: _("Deleting selected item(s)"),
	mode: "remote",
	rememberSelected: false,
	autoReload: false,
    reloadInterval: 1000, // 10 seconds

	initComponent: function() {
		var me = this;
		// Initialize toolbars.
		me.dockedItems = [];
		if(!me.hideTopToolbar) {
			me.dockedItems.push(me.topToolbar = Ext.widget({
				xtype: "toolbar",
				dock: "top",
				items: me.getTopToolbarItems(me)
			}));
		}
		if(!me.hidePagingToolbar) {
			me.dockedItems.push({
				xtype: "toolbar",
				dock: "bottom",
				items: [ me.pagingToolbar = Ext.widget({
					xtype: "pagingtoolbar",
					store: me.store,
					displayInfo: true,
					displayMsg: _("Displaying items {0} - {1} of {2}"),
					emptyMsg: _("No items to display")
				}) ]
			});
		}
		me.callParent(arguments);
		// Register event handler.
		// Process double clicks in grid.
		me.on("itemdblclick", me.onItemDblClick, me);
		// Process selections in grid, e.g. to update the toolbar.
		var selModel = me.getSelectionModel();
		selModel.on("selectionchange", me.onSelectionChange, me);
		// Remember selection to restore it after the grid has been
		// refreshed.
		if(me.rememberSelected) {
			me.getStore().on("beforeload", function() {
				if(!me.rendered || Ext.isEmpty(me.getEl()))
					return;
				if(!selModel.hasSelection())
					return;
				me.previousSelected = selModel.getSelection();
			});
			me.getView().on("refresh", function(view) {
				if(Ext.isEmpty(me.previousSelected))
					return;
				var select = [];
				Ext.Array.each(me.previousSelected, function(r) {
					var record = me.getStore().getById(r.getId());
					if(!Ext.isEmpty(record))
						select.push(record);
				});
				delete me.previousSelected;
				if(select.length > 0) {
					selModel.select(select, false, false);
					selModel.view.focusNode(select[0]);
				}
			});
		}
        // Auto-reload the grid content?
        if(me.autoReload) {
            var fnActivateTask = function() {
                if(Ext.isEmpty(me.reloadTask)) {
                    me.reloadTask = Ext.util.TaskManager.start({
                        run: me.doReload,
                        scope: me,
                        interval: me.reloadInterval,
                        fireOnStart: true
                    });
                }
            };
            var fnDeactivateTask = function() {
                if(!Ext.isEmpty(me.reloadTask)) {
                    Ext.util.TaskManager.stop(me.reloadTask);
                    delete me.reloadTask;
                }
            }
            me.on({
                render: fnActivateTask,
                beforedestroy: fnDeactivateTask,
                activate: fnActivateTask,
                deactivate: fnDeactivateTask,
                scope: me
            });
        }
	},

	/**
	 * Returns the items displayed in the top toolbar.
	 * @param c This component object.
	 * @return An array of buttons displayed in the top toolbar.
	 */
	getTopToolbarItems: function(c) {
		var me = this;
		return [{
			id: me.getId() + "-add",
			xtype: "button",
			text: me.addButtonText,
			icon: "images/zfs_addpool.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			hidden: me.hideAddButton,
			handler: Ext.Function.bind(me.onAddButton, me, [ me ]),
			scope: me
		},{
			id: me.getId() + "-import",
			xtype: "button",
			text: me.importPoolButtonText,
			icon: "images/zfs_import.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			hidden: me.hideImportPoolButton,
			handler: Ext.Function.bind(me.onImportPoolButton, me, [ me ]),
			scope: me
		},{
			id: me.getId() + "-edit",
			xtype: "button",
			text: me.editButtonText,
			icon: "images/edit.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			hidden: me.hideEditButton,
			handler: Ext.Function.bind(me.onEditButton, me, [ me ]),
			scope: me,
			disabled: true
		},{
			id: me.getId() + "-addobj",
			xtype: "button",
			text: me.addObjButtonText,
			icon: "images/zfs_addobject.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			hidden: me.hideAddObjButton,
			handler: Ext.Function.bind(me.onAddObjButton, me, [ me ]),
			scope: me,
			disabled: true
		},{
			id: me.getId() + "-expand",
			xtype: "button",
			text: me.expandPoolButtonText,
			icon: "images/zfs_expand.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			hidden: me.hideExpandPoolButton,
			handler: Ext.Function.bind(me.onExpandPoolButton, me, [ me ]),
			scope: me,
			disabled: true
		},{
			id: me.getId() + "-scrub",
			xtype: "button",
			text: me.scrubButtonText,
			icon: "images/zfs_scrub.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			hidden: me.hideScrubButton,
			handler: Ext.Function.bind(me.onScrubButton, me, [ me ]),
			scope: me,
			disabled: true
		},{
			id: me.getId() + "-export",
			xtype: "button",
			text: me.exportPoolButtonText,
			icon: "images/zfs_export.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			hidden: me.hideExportPoolButton,
			handler: Ext.Function.bind(me.onExportPoolButton, me, [ me ]),
			scope: me,
			disabled: true
		},{
			id: me.getId() + "-rename",
			xtype: "button",
			text: me.renameButtonText,
			icon: "images/zfs_rename.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			hidden: me.hideRenameButton,
			handler: Ext.Function.bind(me.onRenameButton, me, [ me ]),
			scope: me,
			disabled: true
		},{
			id: me.getId() + "-delete",
			xtype: "button",
			text: me.deleteButtonText,
			icon: "images/delete.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			hidden: me.hideDeleteButton,
			handler: Ext.Function.bind(me.onDeleteButton, me, [ me ]),
			scope: me,
			disabled: true
		},{
			id: me.getId() + "-rollback",
			xtype: "button",
			text: me.rollbackButtonText,
			icon: "images/undo.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			hidden: me.hideDeleteButton,
			handler: Ext.Function.bind(me.onRollbackButton, me, [ me ]),
			scope: me,
			disabled: true
		},{
			id: me.getId() + "-up",
			xtype: "button",
			text: me.upButtonText,
			icon: "images/arrow-up.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			hidden: me.hideUpButton,
			handler: Ext.Function.bind(me.onUpButton, me, [ me ]),
			scope: me,
			disabled: true
		},{
			id: me.getId() + "-down",
			xtype: "button",
			text: me.downButtonText,
			icon: "images/arrow-down.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			hidden: me.hideDownButton,
			handler: Ext.Function.bind(me.onDownButton, me, [ me ]),
			scope: me,
			disabled: true
		},{
			id: me.getId() + "-apply",
			xtype: "button",
			text: me.applyButtonText,
			icon: "images/checkmark.png",
			hidden: me.hideApplyButton,
			handler: Ext.Function.bind(me.onApplyButton, me, [ me ]),
			scope: me
		},{
			id: me.getId() + "-refresh",
			xtype: "button",
			text: me.refreshButtonText,
			icon: "images/refresh.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			hidden: me.hideRefreshButton,
			handler: Ext.Function.bind(me.onRefreshButton, me, [ me ]),
			scope: me
		}]
	},

	/**
	 * Handler that is called whenever the selection in the grid has
	 * been changed. The top toolbar buttons will be enabled/disabled
	 * depending on how much rows has been selected.
	 * @param model The selection model
	 */
	onSelectionChange: function(model, records) {
		var me = this;
		if(me.hideTopToolbar)
			return;
		var tbarBtnName = [ "addobj", "edit", "delete", "up", "down", "expand", "scrub", "export", "rename","rollback" ];
		var tbarBtnHidden = {
			"addobj": true,
			"edit": true,
			"delete": true,
			"expand": true,
			"scrub": true,
			"export": true,
			"rename": true,
			"up": true,
			"down": true,
			"rollback": true
		};
		// Enable/disable buttons depending on the number of selected rows.
		if(records.length <= 0) {
          	tbarBtnHidden["addobj"] = true;
			tbarBtnHidden["edit"] = true;
			tbarBtnHidden["delete"] = true;
			tbarBtnHidden["expand"] = true;
			tbarBtnHidden["scrub"] = true;
			tbarBtnHidden["export"] = true;
			tbarBtnHidden["rename"] = true;
			tbarBtnHidden["up"] = true;
			tbarBtnHidden["down"] = true;
			tbarBtnHidden["rollback"] = true;
		} else if(records.length == 1) {
			tbarBtnHidden["addobj"] = false;
			tbarBtnHidden["edit"] = false;
			tbarBtnHidden["delete"] = false;
			tbarBtnHidden["expand"] = false;
			tbarBtnHidden["scrub"] = false;
			tbarBtnHidden["export"] = false;
			tbarBtnHidden["rename"] = false;
			tbarBtnHidden["up"] = true;
			tbarBtnHidden["down"] = true;
			tbarBtnHidden["rollback"] = true;
			// Disable 'Delete' button if selected node is not a leaf
			Ext.Array.each(records, function(record) {
				if(false === record.get("leaf")) {
					tbarBtnHidden["delete"] = true;
					return false;
				}
			});
			// Disable 'Rename' button if selected node is a Pool
			Ext.Array.each(records, function(record) {
				if("Pool" === record.get("type")) {
					tbarBtnHidden["rename"] = true;
					return false;
				}
			});
			//Disable buttons if selected row is not a Pool
			Ext.Array.each(records, function(record) {
				if("Pool" !== record.get("type")) {
					tbarBtnHidden["expand"] = true;
					tbarBtnHidden["scrub"] = true;
					tbarBtnHidden["export"] = true;
					return false;
				}
			});
			//Enable 'Rollback' if selected row is Snapshot
			Ext.Array.each(records, function(record) {
				if("Snapshot" === record.get("type")) {
					tbarBtnHidden["rollback"] = false;
					return false;
				}
			});
		} else {
			tbarBtnHidden["addobj"] = true;
			tbarBtnHidden["edit"] = true;
			tbarBtnHidden["delete"] = false;
			tbarBtnHidden["expand"] = true;
			tbarBtnHidden["scrub"] = true;
			tbarBtnHidden["export"] = true;
			tbarBtnHidden["rename"] = true;
			tbarBtnHidden["up"] = true;
			tbarBtnHidden["down"] = true;
			tbarBtnHidden["rollback"] = true;
		}
		
		// Update the button controls.
		Ext.Array.each(tbarBtnName, function(name) {
			var tbarBtnCtrl = me.queryById(me.getId() + "-" + name);
			if(!Ext.isEmpty(tbarBtnCtrl)) {
				if(true == tbarBtnHidden[name]) {
					tbarBtnCtrl.disable();
					tbarBtnCtrl.hide();
				} else {
					tbarBtnCtrl.enable();
					tbarBtnCtrl.show();
				}
			}
		});
	},

	onItemDblClick: function() {
		var me = this;
		if(!me.hideTopToolbar && !me.hideEditButton) {
			me.onEditButton(me);
		}
	},

	/**
	 * Load the grid content.
	 */
	doLoad: function() {
		var me = this;
		if(me.mode === "remote") {
			me.store.load();
		}
	},

	/**
	 * Reload the grid content.
	 */
	doReload: function() {
		var me = this;
		if(me.mode === "remote") {
			me.store.reload();
		}
	},

	/**
	 * Handler that is called when the 'Add' button in the top toolbar
	 * is pressed. Override this method to customize the default behaviour.
	 * @param this The grid itself.
	 */
	onAddButton: function() {
		// Nothing to do here
	},

	/**
	 * * Handler that is called when the 'AddObj' button in the top toolbar
	 * is pressed. Override this method to customize the default behaviour.
	 * @param this The grid itself.
	 */
	onAddObjButton: function() {
		// Nothing to do here
	},


	/**
	 * Handler that is called when the 'Edit' button in the top toolbar
	 * is pressed. Override this method to customize the default behaviour.
	 * @param this The grid itself.
	 */
	onEditButton: function() {
		// Nothing to do here
	},

	/**
	 * Handler that is called when the 'Up' button in the top toolbar
	 * is pressed. Override this method to customize the default behaviour.
	 * @param this The grid itself.
	 */
	onUpButton: function() {
		var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		if(records.length > 0) {
			// Find the smallest index of the selected rows.
			var ltIndex = me.store.indexOf(records[0]);
			Ext.Array.each(records, function(record) {
				var index = me.store.indexOf(record);
				if(ltIndex > index)
					ltIndex = index;
			});
			// Calculate the index where to insert the rows.
			var index = ltIndex - 1;
			if(index < 0)
				index = 0;
			me.doMoveRows(records, index);
		}
	},

	/**
	 * Handler that is called when the 'Down' button in the top toolbar
	 * is pressed.
	 * @param this The grid itself.
	 */
	onDownButton: function() {
		var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		if(records.length > 0) {
			// Find the smallest index of the selected rows.
			var ltIndex = me.store.indexOf(records[0]);
			Ext.Array.each(records, function(record) {
				var index = me.store.indexOf(record);
				if(ltIndex > index)
					ltIndex = index;
			});
			// Calculate the index where to insert the rows.
			var index = ltIndex + records.length;
			var count = me.store.getCount() - 1;
			if(index > count)
				index = count;
			me.doMoveRows(records, index);
		}
	},

	/**
	 * Handler that is called when the 'Apply' button in the top toolbar
	 * is pressed. Override this method to customize the default behaviour.
	 * @param this The grid itself.
	 */
	onApplyButton: function() {
		// Nothing to do here
	},

	/**
	 * Handler that is called when the 'Refresh' button in the top toolbar
	 * is pressed. Override this method to customize the default behaviour.
	 * @param this The grid itself.
	 */
	onRefreshButton: function() {
		this.doReload();
	},

	/**
	 * Move the given rows to the given index.
	 * @param records The records to move.
	 * @param index The index where to insert the rows to be moved.
	 */
	doMoveRows: function(records, index) {
		var me = this;
		if(!Ext.isNumber(index))
			return;
		records = Ext.Array.from(records);
		me.store.suspendEvents();
		Ext.Array.each(records, function(record) {
			me.store.remove(record);
			me.store.insert(index, record);
		});
		me.store.resumeEvents();
		me.afterMoveRows(records, index);
		me.getView().refresh();
	},

	/**
	 * Function that is called after the selected rows have been moved.
	 * Override this method to customize the default behaviour.
	 * @param records The records that have been move.
	 * @param index The index where the rows have been inserted.
	 */
	afterMoveRows: function(records, index) {
		var sm = this.getSelectionModel();
		sm.select(records);
	},

	/**
	 * Handler that is called when the 'Delete' button in the top toolbar
	 * is pressed.
	 */
	onDeleteButton: function() {
		var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		if(me.deletionConfirmRequired === true) {
			var msg = _("Do you really want to delete the selected item(s)?");
			OMV.MessageBox.show({
				title: _("Confirmation"),
				msg: msg,
				buttons: Ext.Msg.YESNO,
				fn: function(answer) {
					if(answer !== "yes")
						return;
					me.startDeletion(records);
				},
				scope: me,
				icon: Ext.Msg.QUESTION
			});
		} else {
			me.startDeletion(records);
		}
	},

	/**
	 * Handler for the settings button. Pops up a dialog displaying
	 * general settings for ZFS Override this method to customize the
	 * default behavior.
	 * @param this The grid itself.
	 */
	onSettingsButton: function() {
		// Nothing to do here
	},

	/**
	 * @private
	 * Private method that is called when the deletion of the selected records
	 * has been aggreed.
	 * @param records The records to delete.
	 */
	startDeletion: function(records) {
		var me = this;
		if(records.length <= 0)
			return;
		// Store selected records in a local variable
		me.delActionInfo = {
			records: records,
			count: records.length
		}
		// Get first record to be deleted
		var record = me.delActionInfo.records.pop();
		// Display progress dialog
		OMV.MessageBox.progress("", me.deletionWaitMsg, "");
		me.updateDeletionProgress();
		// Execute deletion function
		me.doDeletion(record);
	},

	/**
	 * The method that is called to delete a selected record. Override this
	 * method to customize the default behaviour. This is necessary in
	 * 'remote' mode.
	 */
	doDeletion: function(record) {
		var me = this;
		if(me.mode === "local") {
			// Remove record from store
			me.store.remove(record);
			// Continue deletion process
			me.onDeletion(null, true, null);
		}
	},

	/**
	 * The method that is called by the 'doDeletion' method. The progress
	 * bar will be updated and the deletion progress will be continued if
	 * there are still records to delete.
	 */
	onDeletion: function(id, success, response) {
		var me = this;
		if(!success) {
			// Remove temporary local variables
			delete me.delActionInfo;
			// Hide progress dialog
			OMV.MessageBox.hide();
			// Display error message
			OMV.MessageBox.error(null, response);
		} else {
			if(me.delActionInfo.records.length > 0) {
				var record = me.delActionInfo.records.pop();
				// Update progress dialog
				me.updateDeletionProgress();
				// Execute deletion function
				me.doDeletion(record);
			} else {
				// Remove temporary local variables
				delete me.delActionInfo;
				// Update and hide progress dialog
				OMV.MessageBox.updateProgress(1, _("100% completed ..."));
				OMV.MessageBox.hide();
				me.afterDeletion();
			}
		}
	},

	/**
	 * Function that is called after the deletion has been successful finished.
	 */
	afterDeletion: function() {
		var me = this;
		var sm = me.getSelectionModel();
		sm.deselectAll();
		if(me.mode === "remote") {
			me.doReload();
		};
	},

	/**
	 * @private
	 * Private helper function to update the progress dialog.
	 */
	updateDeletionProgress: function() {
		var me = this;
		// Calculate percentage
		var p = (me.delActionInfo.count - me.delActionInfo.records.length) /
			me.delActionInfo.count;
		// Create message text
		var text = Math.round(100 * p) + _("% completed ...");
		// Update progress dialog
		OMV.MessageBox.updateProgress(p, text);
	},

	/**
	 * Convenience function for setting the given toolbar button
	 * disabled/enabled.
	 * @param name The name of the toolbar button.
	 * @param disabled TRUE to disable the button, FALSE to enable.
	 * @return The button component, otherwise FALSE.
	 */
	setToolbarButtonDisabled: function(name, disabled) {
		var me = this;
		var result = false;
		var btnCtrl = me.queryById(me.getId() + "-" + name);
		if(!Ext.isEmpty(btnCtrl) && btnCtrl.isButton)
			result = btnCtrl.setDisabled(disabled);
		return result;
	},

	/**
	 * Helper function to get the top toolbar object.
	 * @return The paging toolbar object or NULL.
	 */
	getTopToolbar: function() {
		return this.topToolbar;
	},

	/**
	 * Helper function to get the paging toolbar object.
	 * @return The paging toolbar object or NULL.
	 */
	getPagingToolbar: function() {
		return this.pagingToolbar;
	}
});



