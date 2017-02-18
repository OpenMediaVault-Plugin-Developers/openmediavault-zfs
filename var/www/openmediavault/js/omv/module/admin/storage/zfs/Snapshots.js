// require("js/omv/WorkspaceManager.js")
// require("js/omv/workspace/grid/Panel.js")
// require("js/omv/Rpc.js")
// require("js/omv/data/Store.js")
// require("js/omv/data/Model.js")
// require("js/omv/data/proxy/Rpc.js")
// require("js/omv/workspace/window/plugin/ConfigObject.js")
// require("js/omv/module/admin/storage/zfs/Detail.js")

Ext.define("OMV.module.admin.storage.zfs.Snapshots", {
	extend: "OMV.workspace.grid.Panel",
	requires: [
		"OMV.Rpc",
		"OMV.data.Store",
		"OMV.data.Model",
		"OMV.data.proxy.Rpc",
		"OMV.workspace.window.plugin.ConfigObject"
	],

	hidePagingToolbar : false,
	hideAddButton     : true,
	hideEditButton    : true,
	hideDeleteButton  : false,
	rememberSelected  : true,
	stateful: true,
	stateId: "d14ac294-f5bd-11e6-a856-4a49d0dfdf2b",
	selModel: "checkboxmodel",
	columns: [{
		text: _("Filesystem/Voulume"),
		dataIndex: 'parent',
		sortable: true,
		stateId: 'parent',
	},{
		text: _("Name"),
		dataIndex: 'name',
		sortable: true,
		stateId: 'name',
	},{
		text: _("Used"),
		dataIndex: 'used',
		sortable: true,
		stateId: 'used'
	},{
		text: _("Refer"),
		dataIndex: 'refer',
		sortable: true,
		stateId: 'available'
	},{
		text: _("Details"),
		title: 'Details',
		xtype: 'actioncolumn',
		tooltip: 'Details',
		align: 'center',
		icon: 'images/search.png',
		handler: function(view, rowIndex, colIndex, item, e, record, row) {
			var me = this;
			Ext.create("OMV.module.admin.storage.zfs.ShowDetails", {
				title: _("Object details"),
				rpcGetMethod: "getObjectDetails",
				rpcGetParams: {
					name: record.get('path'),
					type: record.get('type')
				}
			}).show();
		}
	}],

	initComponent: function() {
		var me = this;
		Ext.apply(me, {
			store: Ext.create("OMV.data.Store", {
				autoLoad: true,
				model: OMV.data.Model.createImplicit({
					idProperty: "id",
					fields: [
						{ name: "id", type: "string" },
						{ name: "name", type: "string" },
						{ name: "parent", type: "string" },
						{ name: "used", type: "string" },
						{ name: "refer", type: "string" },
						{ name: "path", type: "string" },
						{ name: "type", type: "string" }
					]
				}),
				proxy: {
					type: "rpc",
					rpcData: {
						service: "ZFS",
						method: "getAllSnapshots"
					}
				},
				remoteSort: false,
				sorters: [{
					direction: "ASC",
					property: "parent"
				},{
					direction: "ASC",
					property: "name"
				}]
			})
		});
		me.callParent(arguments);
	},

	getTopToolbarItems: function() {
		var me = this;
		var items = me.callParent(arguments);
		Ext.Array.erase(items, 2, 1);
		Ext.Array.insert(items,0, [{
			id: me.getId() + "-delete",
			xtype: "button",
			text: _("Delete"),
			icon: "images/delete.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			handler: Ext.Function.bind(me.onDeleteButton, me, [ me ]),
			scope: me,
			disabled: true,
			selectionConfig: {
				minSelections: 1
			}
		},{
			id: me.getId() + "-clone",
			xtype: "button",
			text: _("Clone"),
			icon: "images/zfs_rename.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			handler: Ext.Function.bind(me.onCloneButton, me, [ me ]),
			scope: me,
			disabled: true,
			hidden: true,
			selectionConfig: {
				minSelections: 1,
				maxSelections: 1
			}
		},{
			id: me.getId() + "-rollback",
			xtype: "button",
			text: _("Rollback"),
			icon: "images/undo.png",
			iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
			handler: Ext.Function.bind(me.onRollbackButton, me, [ me ]),
			scope: me,
			disabled: true,
			selectionConfig: {
				minSelections: 1,
				maxSelections: 1
			}
		}]);
		return items;
	},

	doDeletion: function(record) {
		var me = this;
		OMV.Rpc.request({
			scope: me,
			callback: me.onDeletion,
			rpcData: {
				service: "ZFS",
				method: "deleteObject",
				params: {
					name: record.get('path'),
					type: record.get('type')
				}
			}
		});
	},

	onCloneButton: function(record) {
		var me = this;
		OMV.Rpc.request({
			scope: me,
			callback: me.onDeletion,
			rpcData: {
				service: "ZFS",
				method: "deleteObject",
				params: {
					name: record.get('path'),
					type: record.get('type')
				}
			}
		});
	},
	
	onRollbackButton: function() {
		var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		var record = records[0];
		var msg = _("Do you really want to rollback the selected item?");
		OMV.MessageBox.show({
			title: _("Confirmation"),
			msg: msg,
			buttons: Ext.Msg.YESNO,
			fn: function(answer) {
				if(answer !== "yes")
					return;
				var me = this;
				OMV.Rpc.request({
					scope: me,
					callback: function(id, success, response) {
						if(success) {
							OMV.MessageBox.success(null, _("Successfully rollbacked!"));
							me.doReload();
						} else {
							OMV.MessageBox.error(null, response);
						}
                    },
					rpcData: {
						service: "ZFS",
						method: "rollbackSnapshot",
						params: {
							name: record.get('path')
						}
					}
				});
			},
			scope: me,
			icon: Ext.Msg.QUESTION
		});
	},
});

OMV.WorkspaceManager.registerPanel({
	id: "snapshots",
	path: "/storage/zfs",
	text: _("Snapshots"),
	position: 20,
	className: "OMV.module.admin.storage.zfs.Snapshots"
});
