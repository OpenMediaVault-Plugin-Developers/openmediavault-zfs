// require("js/omv/tree/Panel.js")
// require("js/omv/module/admin/storage/zfs/TreePanel.js")
// require("js/omv/workspace/window/Grid.js")

Ext.define("OMV.module.admin.storage.zfs.AddObject", {
	extend: "OMV.workspace.window.Form",
	uses: [
		"OMV.data.Store",
		"OMV.data.Model",
		"OMV.data.proxy.Rpc",
		"OMV.data.reader.RpcArray"
	],

	rpcService: "ZFS",
	rpcSetMethod: "addObject",
	width: 420,

	getFormItems: function() {
		var me = this;
		return [{
			xtype: "combo",
			name: "type",
			fieldLabel: _("Object Type"),
			queryMode: "local",
			store: [
				[ "filesystem", "Filesystem" ],
				[ "snapshot", "Snapshot" ],
				[ "volume", "Volume" ]
			],
			allowBlank: true,
			editable: false,
			triggerAction: "all",
			value: "filesystem",
			listeners: {
				scope: me,
				change: function(combo, value) {
					var sizeField = this.findField("size");
					switch(value) {
						case "volume":
							sizeField.show();
						sizeField.allowBlank = false;
						break;
						default:
							sizeField.hide();
						sizeField.allowBlank = true;
						break;
					}
					sizeField.validate();
				}
			}
		},{
			xtype: "textfield",
			name: "path",
			fieldLabel: _("Prefix"),
			allowBlank: false,
			readOnly: true
		},{
			xtype: "textfield",
			name: "name",
			fieldLabel: _("Name"),
			allowBlank: false,
			plugins: [{
				ptype: "fieldinfo",
				text: _("Name of the new object. Prefix will prepend the name. Please omit leading /")
			}]
		},{
			xtype: "textfield",
			name: "size",
			hidden: true,
			fieldLabel: _("Size"),
			allowBlank: true,
			plugins: [{
				ptype: "fieldinfo",
				text: _("Size of the volume e.g. 5mb,100gb,1tb etc")
			}]
		}];
	}
});



Ext.define("OMV.module.admin.storage.zfs.EditProperties", {
	extend: "OMV.workspace.window.Grid",
	requires: [
		"OMV.data.Store",
		"OMV.data.Model",
		"OMV.data.proxy.Rpc"
	],

	rpcService: "ZFS",
	rpcSetMethod: "setProperties",

	title: _("Edit properties"),
	width: 500,
	height: 305,

	getGridConfig: function() {
		var me = this;

		var rowEditing = Ext.create('Ext.grid.plugin.RowEditing', {
			clicksToEdit: 1,
			pluginId: 'rowEditing',
			listeners: {
				validateedit: function(editor, e, eOpts) {
					e.record.set("modified", "true");
				},
				beforeedit: function(editor, e, eOpts) {
					if (e.record.get("property") === "mountpoint") {
						e.grid.getPlugin('rowEditing').editor.form.findField("value").disable();
						e.grid.getPlugin('rowEditing').editor.form.findField("property").disable();
					} else if (e.record.get("newproperty") === "false") {
						e.grid.getPlugin('rowEditing').editor.form.findField("value").enable();
						e.grid.getPlugin('rowEditing').editor.form.findField("property").disable();
					} else {
						e.grid.getPlugin('rowEditing').editor.form.findField("value").enable();
						e.grid.getPlugin('rowEditing').editor.form.findField("property").enable();
					}
				}

			}
		});

		var store = Ext.create("OMV.data.Store", {
			autoLoad: true,
			model: OMV.data.Model.createImplicit({
				fields: [
					{ name: "property", type: "string" },
					{ name: "value", type: "string" },
					{ name: "source", type: "string" },
					{ name: "modified", type: "string" },
					{ name: "newproperty", type: "string", defaultValue: "false" }
				]
			}),
			proxy: {
				type: "rpc",
				rpcData: {
					service: "ZFS",
					method: "getProperties",
					params: {
						name: me.name,
						type: me.type
					}
				}
			}
		});

		return {
			border: false,
			stateful: true,
			stateId: "8c3dc800-bdbb-11e3-b1b6-0800200c9a66",
			selType: 'rowmodel',
			plugins: [rowEditing],
			store: store,
			tbar: [{
				text: "Add property",
				icon: "images/add.png",
				iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
				handler: function(view) {
					Ext.define('Property', {
						extend: 'Ext.data.Model',
						fields: [
							"property",
							"value",
							"source",
							"modified",
							"newproperty"
						]
					});
					var newProperty = Ext.create("Property", {
						property: "",
						value: "",
						source: "local",
						modified: "true",
						newproperty: "true"
					});
					rowEditing.cancelEdit();
					store.insert(0, newProperty);
					rowEditing.startEdit();
				}
			}],
			columns: [{
				text: _("Property"),
				sortable: true,
				dataIndex: "property",
				stateId: "property",
				editor: {
					xtype: "textfield",
					allowBlank: false,
				}
			},{
				text: _("Value"),
				sortable: true,
				dataIndex: "value",
				stateId: "value",
				flex: 1,
				readOnly: true,
				editor: {
					xtype: "textfield",
					allowBlank: false,
				}
			},{
				text: _("Source"),
				sortable: true,
				dataIndex: "source",
				stateId: "source",
			},{
				xtype: 'actioncolumn',
				header: 'Inherit',
				icon: "images/checkmark.png",
				tooltip: "Inherit",
				handler: function(view, rowIndex, colIndex, item, e, record, row) {
					OMV.RpcObserver.request({
						msg     : _("Updating property..."),
						rpcData : {
							service: "ZFS",
							method: "inherit",
							params: {
								name: me.name,
								type: me.type,
								property: record.get("property")
							}
						},
						finish  : function() {
							view.getStore().reload();
						}
					});
				},
				isDisabled: function(view, rowIdx, colIdx, item, record) {
					var src = record.get("source");
					if(src === "local") {
						return false;
					} else {
						return true;
					}
				}
			},{
				text: _("New"),
				dataIndex: "newproperty",
				stateId: "newproperty",
				sortable: false,
				hidden: true
			},{
				text: _("Modified"),
				sortable: false,
				dataIndex: "modified",
				stateId: "modified",
				hidden: true
			}],
		};
	},

	getRpcSetParams: function() {
		var me = this;
		var properties = [];
		var values = me.getValues();
		Ext.Array.each(values, function(value) {
			if(value.modified === "false")
				return;
			properties.push({
				"property": value.property,
				"value": value.value,
			});
		});
		return {
			name: me.name,
			type: me.type,
			properties: properties
		};
	}

});


Ext.define("OMV.module.admin.storage.zfs.CreateShare", {
	extend: "OMV.workspace.window.Form",
	uses: [
		"OMV.data.Store",
		"OMV.data.Model",
		"OMV.data.proxy.Rpc",
		"OMV.data.reader.RpcArray"
	],

	rpcService: "ZFS",
	rpcSetMethod: "createShare",
	width: 500,

	getFormItems: function() {
		var me = this;
		return [{
			xtype: "textfield",
			name: "sharename",
			fieldLabel: _("Name"),
			allowBlank: false,
		},{
			xtype: "textfield",
			name: "mountpoint",
			fieldLabel: _("Path"),
			allowBlank: false,
			readOnly: true
		},{
			xtype: "combo",
			name: "mode",
			fieldLabel: _("Permissions"),
			queryMode: "local",
			store: Ext.create("Ext.data.ArrayStore", {
				fields: [ "value", "text" ],
				data: [
					[ "700", _("Administrator: read/write, Users: no access, Others: no access") ],
					[ "750", _("Administrator: read/write, Users: read-only, Others: no access") ],
					[ "770", _("Administrator: read/write, Users: read/write, Others: no access") ],
					[ "755", _("Administrator: read/write, Users: read-only, Others: read-only") ],
					[ "775", _("Administrator: read/write, Users: read/write, Others: read-only") ],
					[ "777", _("Everyone: read/write") ]
				]
			}),
			displayField: "text",
			valueField: "value",
			allowBlank: false,
			editable: false,
			showItemTooltip: true,
			triggerAction: "all",
			value: "775",
			plugins: [{
				ptype: "fieldinfo",
				text: _("The file mode of the shared folder path.")
			}]
		},{
			xtype: "textarea",
			name: "comment",
			fieldLabel: _("Comment"),
			allowBlank: true
		},{
			xtype: "textarea",
			name: "name",
			hidden: true
		},{
			xtype: "textarea",
			name: "type",
			hidden: true
		}];
	}
});



Ext.define("OMV.module.admin.storage.zfs.Overview", {
	extend: "OMV.module.admin.storage.zfs.TreePanel",

	rpcService: "ZFS",
	rpcGetMethod: "getObjectTree",
	requires: [
		"OMV.data.Store",
		"OMV.data.Model",
		"OMV.data.proxy.Rpc"
	],

	rootVisible: false,
	stateful: true,
	stateId: "cec54550-bc2a-11e3-a5e2-0800200c9a66",

	columns: [{
		text: _("Name"),
		xtype: 'treecolumn',
		dataIndex: 'name',
		sortable: true,
		flex: 2,
		stateId: 'name'
	},{
		text: _("Type"),
		dataIndex: 'type',
		sortable: true,
		flex: 1,
		stateId: 'type'
	},{
		text: _("Share"),
		xtype: 'actioncolumn',
		tooltip: 'Create shared folder',
		align: 'center',
		icon: 'images/checkmark.png',
		handler: function(view, rowIndex, colIndex, item, e, record, row) {
			var me = this;
			Ext.create("OMV.module.admin.storage.zfs.CreateShare", {
				title: _("Create shared folder"),
				rpcGetMethod: "getSharedParams",
				rpcGetParams: {
					name: record.get('path'),
					type: record.get('type')
				}
			}).show();
		},
		isDisabled: function(view, rowIdx, colIdx, item, record) {
			var src = record.get("type");
			if((src === "Filesystem") && (record.get("shared") === "false")) {
				return false;
			} else {
				return true;
			}
		}


	},{
		text: _("Details"),
		xtype: 'actioncolumn',
		tooltip: 'Details',
		align: 'center',
		icon: 'images/zfs_mag.png'
	},{
		text: _("Shared"),
		dataIndex: 'shared',
		sortable: false,
		stateId: 'shared',
		hidden: true
	}],

	initComponent: function() {
		var me = this;
		this.width = 600;
		Ext.apply(me, {
			store: Ext.create("Ext.data.TreeStore", {
				autoLoad: true,
				model: OMV.data.Model.createImplicit({
					fields: [
						{ name: "name", type: "string" },
						{ name: "type", type: "string" },
						{ name: "id", type: "string" },
						{ name: "path", type: "string" },
						{ name: "origin", type: "string", defaultValue: "none" },
						{ name: "shared", type: "string", defaultValue: "false" }
					]
				}),
				proxy: {
					type: "rpc",
					rpcData: {
						service: "ZFS",
						method: "getObjectTree",
					}
				},
				folderSort: true
			})
		});
		me.callParent(arguments);
	},

	onAddObjButton: function() {
		var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		var record = records[0];
		Ext.create("OMV.module.admin.storage.zfs.AddObject", {
			title: _("Add Object"),
			rpcGetMethod: "passParam",
			rpcGetParams: {
				key: "path",
				value: record.get('path')
			},
			listeners: {
				scope: me,
				submit: function() {
					this.doReload();
				}
			}
		}).show();
	},

	onEditButton: function() {
		var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		var record = records[0];
		Ext.create("OMV.module.admin.storage.zfs.EditProperties", {
			name: record.get("path"),
			type: record.get("type")
		}).show();
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
	}

});

OMV.WorkspaceManager.registerPanel({
	id: "overview",
	path: "/storage/zfs",
	text: _("Overview"),
	position: 10,
	className: "OMV.module.admin.storage.zfs.Overview"
});



