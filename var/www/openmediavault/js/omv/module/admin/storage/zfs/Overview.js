// require("js/omv/WorkspaceManager.js")
// require("js/omv/workspace/window/Grid.js")
// require("js/omv/workspace/window/Form.js")
// require("js/omv/data/Store.js")
// require("js/omv/data/Model.js")
// require("js/omv/data/proxy/Rpc.js")
// require("js/omv/tree/Panel.js")
// require("js/omv/form/field/CheckboxGrid.js")
// require("js/omv/module/admin/storage/zfs/TreePanel.js")
// require("js/omv/module/admin/storage/zfs/PoolMgmt.js")
// require("js/omv/module/admin/storage/zfs/ObjMgmt.js")

Ext.define("OMV.module.admin.storage.zfs.ShowDetails", {
	extend: "OMV.workspace.window.Form",
	requires: [
		"OMV.data.Store",
		"OMV.data.Model",
		"OMV.data.proxy.Rpc",
	],

	rpcService: "ZFS",
	title: _("Object details"),
	autoLoadData: true,
	hideResetButton: true,
	hideCancelButton: true,
	width: 700,
	height: 350,
	layout: 'fit',
	okButtonText: _("Ok"),

	getFormItems: function() {
		var me = this;
		
		return [{
			xtype: "textareafield",
			name: "details",
			grow: true,
			readOnly: true,
			fieldStyle: {
				fontFamily: "courier",
				fontSize: "12px"
			}
		}];
	}
});

/**
 * @class OMV.module.admin.storage.zfs.Settings
 * @derived OMV.workspace.window.Form
 *
 * Edit general ZFS settings.
 */
Ext.define("OMV.module.admin.storage.zfs.Settings", {
	extend: "OMV.workspace.window.Form",

	title: _("Settings"),
	hideTopToolbar: true,
	rpcService: "ZFS",
	rpcGetMethod: "getSettings",
	rpcSetMethod: "setSettings",

	plugins: [{
		ptype: "linkedfields",
		correlations: [{
			name: "autoShareNestedProperty",
			conditions: [
				{ name: "autoShareNestedEnabled", value: true }
			],
			properties: ["enabled"]
		},{
			name: "omitShareNestedProperty",
			conditions: [
				{ name: "autoShareNestedEnabled", value: true }
			],
			properties: ["enabled"]
		}]
	}],

	getFormItems: function() {
		var me = this;

		return [{
			xtype: "checkbox",
			name: "autoShareNestedEnabled",
			fieldLabel: _("Enable Automatic Sharing of Nested Properties"),
			checked: true,
			plugins: [{
				ptype: "fieldinfo",
				text: _("When sharing a ZFS filesystem over NFS, normally any " +
				"nested filesystems within the shared filesystem will not be " +
				"shared and, instead, will appear as empty folders. This setting, " +
				"when enabled, will automatically create shares for any nested " +
				"filesystems of this filesystem. Note: this only applies to " +
				"filesystems shared via OMV's shared folders and not those " +
				"shared with the \"sharenfs\" ZFS property.")
			}]
		}, {
			xtype: "textfield",
			name: "autoShareNestedProperty",
			fieldLabel: _("AutoShare Filesystem Property"),
			value: me.autoShareNestedProperty,
			allowBlank: true,
			maxLength: 256,
			regex: new RegExp('[a-z0-9\:\+\._]+\:[a-z0-9\:\+\._]'),
			regexText: _("ZFS user properties may only contain lowercase letters, " +
				"numbers, and the following punctionation: ':','+','.','_'. They " +
				"must also contain at least one ':' character to distinguish them " +
				"from native properties."),
			plugins: [{
				ptype: "fieldinfo",
				text: _("Property to look for on filesystems that, when present, " +
				"will indicate whether this filesystem and it's children should be " +
				"included in automatic sharing. If this is left blank, all " + 
				"shared filesystems will have their nested children automatically " +
				"shared. If it's set, then only filesystems with the property " +
				"set to true will be automatically shared while filesystems " +
				"with this set to false will not be shared. This latter behavior " +
				"is useful when trying to omit a single nested filesystem from being shared " +
				"when it's parent is autosharing it's children. See " +
				"<a href='https://docs.oracle.com/cd/E19120-01/open.solaris/817-2271/gdrcw/index.html'>" +
				"ZFS User Properties</a> for more information.")
			}]
		}]
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
					if (e.record.get("newproperty") === "false") {
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
		stateId: 'name',
		renderer: function(value, p, r){
			if (r.data['origin'] === "n/a") {
				return r.data['name'];
			} else {
				return r.data['name'] + ' (' + r.data['origin'] + ')';
			}
		}
	},{
		text: _("Type"),
		dataIndex: 'type',
		sortable: true,
		flex: 1,
		stateId: 'type',
		renderer: function(value, p, r){
			if (r.data['origin'] === "n/a") {
				return r.data['type'];
			} else {
				return 'Clone';
			}
		}
	},{
		text: _("Size"),
		dataIndex: 'size',
		sortable: true,
		flex: 1,
		stateId: 'size'
	},{
		text: _("Used"),
		dataIndex: 'used',
		sortable: true,
		flex: 1,
		stateId: 'used'
	},{
		text: _("Available"),
		dataIndex: 'available',
		sortable: true,
		flex: 1,
		stateId: 'available'
	},{
		text: _("Mountpoint"),
		dataIndex: 'mountpoint',
		sortable: true,
		flex: 1,
		stateId: 'mountpoint'
	},{
		text: _("State"),
		dataIndex: 'state',
		sortable: true,
		flex: 1,
		stateId: 'state',
		renderer: function(value, p, r){
			if (r.data['state'] === "n/a") {
				return '';
			} else {
				return r.data['state'];
			}
		}
	},{
		text: _("Status"),
		dataIndex: 'status',
		sortable: true,
		stateId: 'status',
		renderer: function(value, p, r){
			if (r.data['status'] === "n/a") {
				return '';
			} else {
				return r.data['status'];
			}
		}
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
		this.width = 600;
		Ext.apply(me, {
			store: Ext.create("Ext.data.TreeStore", {
				autoLoad: true,
				model: OMV.data.Model.createImplicit({
					fields: [
						{ name: "name", type: "string" },
						{ name: "type", type: "string" },
						{ name: "size", type: "string" },
						{ name: "used", type: "string" },
						{ name: "available", type: "string" },
						{ name: "mountpoint", type: "string" },
						{ name: "id", type: "string" },
						{ name: "path", type: "string" },
						{ name: "origin", type: "string", defaultValue: "none" },
						{ name: "lastscrub", type: "string" },
						{ name: "state", type: "string" },
						{ name: "status", type: "string" }
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

	onAddButton: function() {
		var me = this;
		Ext.create("OMV.module.admin.storage.zfs.AddPool", {
			listeners: {
				scope: me,
				submit: function() {
					this.doReload();
				}
			}
		}).show();
	},

	onAddObjButton: function() {
		var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		var record = records[0];
		Ext.create("OMV.module.admin.storage.zfs.AddObject", {
			title: _("Add Object"),
			path: record.get("path"),
			parenttype: record.get("type"),
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
	
	onExpandPoolButton: function() {
		var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		var record = records[0];
		Ext.create("OMV.module.admin.storage.zfs.ExpandPool", {
			title: _("Expand Pool"),
			name: record.get("path"),
			listeners: {
				scope: me,
				submit: function() {
					this.doReload();
				}
			}
		}).show();
	},

    onScrubButton: function() {
        var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		var record = records[0];
        var msg = _("Do you really want to scrub the pool?<br/><br/>Latest scrub: " + record.get('lastscrub'));
        OMV.MessageBox.show({
            title: _("Confirmation"),
            msg: msg,
            buttons: Ext.Msg.YESNO,
            icon: Ext.Msg.QUESTION,
            scope: me,
            fn: function(answer) {
                if(answer !== "yes")
                    return;
                OMV.Rpc.request({
                    scope: me,
                    callback: function(id, success, response) {
                        me.doReload();
                    },
                    relayErrors: false,
                    rpcData: {
                        service: "ZFS",
                        method: "scrubPool",
                        params: {
                            name: record.get("path")
                        }
                    }
                });
            }
        });
    },

	onImportPoolButton: function() {
		var me = this;
		Ext.create("OMV.module.admin.storage.zfs.ImportPool", {
			listeners: {
				scope: me,
				submit: function() {
					this.doReload();
				}
			}
		}).show();
	},
    
	onRenameButton: function() {
		var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		var record = records[0];
		Ext.create("OMV.module.admin.storage.zfs.Rename", {
			oldname: record.get("name"),
			type: record.get("type"),
			oldpath: record.get("path"),
			listeners: {
				scope: me,
				submit: function() {
					this.doReload();
				}
			}
		}).show();
	},
    
	onExportPoolButton: function() {
        var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		var record = records[0];
        var msg = _("Do you really want to export the pool?");
        OMV.MessageBox.show({
            title: _("Confirmation"),
            msg: msg,
            buttons: Ext.Msg.YESNO,
            icon: Ext.Msg.QUESTION,
            scope: me,
            fn: function(answer) {
                if(answer !== "yes")
                    return;
                OMV.Rpc.request({
                    scope: me,
                    callback: function(id, success, response) {
                        me.doReload();
                    },
                    relayErrors: false,
                    rpcData: {
                        service: "ZFS",
                        method: "exportPool",
                        params: {
                            name: record.get("path")
                        }
                    }
                });
            }
        });
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
	
	onSettingsButton: function() {
		var me = this;
		Ext.create("OMV.module.admin.storage.zfs.Settings", {}).show();
	}
});

OMV.WorkspaceManager.registerPanel({
	id: "overview",
	path: "/storage/zfs",
	text: _("Overview"),
	position: 10,
	className: "OMV.module.admin.storage.zfs.Overview"
});
