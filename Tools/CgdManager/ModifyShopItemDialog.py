from PySide6.QtWidgets import (
    QVBoxLayout, QComboBox, QPushButton, QDialog, QFormLayout, QLineEdit,
    QDialogButtonBox, QMessageBox, QInputDialog
)
from Utils import getFieldValue, showToast


class ModifyItemDialog(QDialog):
    BASE_KEYS = ["vi_list_01", "vi_list_02", "vi_list_03", "vi_list_04"]
    VARIANT_SUFFIXES = ["", "_a", "_b", "_c", "_d"]

    def __init__(self, parent, cgdManager, vendor_cdb_name, vendor_entry_index, item_lookup, icon_lookup, item_icon_path, vendor_lookup):
        super().__init__(parent)
        self.cgdManager = cgdManager
        self.vendor_cdb_name = vendor_cdb_name
        self.vendor_entry_index = vendor_entry_index
        self.vendor_entry = self.cgdManager.cdbs[self.vendor_cdb_name].entries[self.vendor_entry_index]
        self.item_lookup = item_lookup
        self.icon_lookup = icon_lookup
        self.item_icon_path = item_icon_path
        self.vendor_lookup = vendor_lookup

        self.setWindowTitle("Modify Item Durations & Prices")
        self.resize(520, 380)

        self.layout = QVBoxLayout(self)
        self.durationDropdown = QComboBox()
        self.dropdown_map = []
        self.rebuildDropdown()
        self.durationDropdown.currentIndexChanged.connect(self.onDurationChanged)
        self.layout.addWidget(self.durationDropdown)

        self.form = QFormLayout()
        self.input_cash = QLineEdit()
        self.input_coupon = QLineEdit()
        self.input_point = QLineEdit()
        self.form.addRow("RockTokens:", self.input_cash)
        self.form.addRow("Coupons:", self.input_coupon)
        self.form.addRow("MicroPoints:", self.input_point)
        
        self.input_item_id = QLineEdit()
        self.input_variant_a = QLineEdit()
        self.input_variant_b = QLineEdit()
        self.input_variant_c = QLineEdit()
        self.input_variant_d = QLineEdit()

        self.form.addRow("Item ID:", self.input_item_id)
        self.form.addRow("Variant _a ID:", self.input_variant_a)
        self.form.addRow("Variant _b ID:", self.input_variant_b)
        self.form.addRow("Variant _c ID:", self.input_variant_c)
        self.form.addRow("Variant _d ID:", self.input_variant_d)

        self.layout.addLayout(self.form)

        self.removeBtn = QPushButton("Remove this duration")
        self.removeBtn.clicked.connect(self.removeCurrentDuration)
        self.layout.addWidget(self.removeBtn)

        self.addDurationBtn = QPushButton("Add New Duration")
        self.addDurationBtn.clicked.connect(self.addNewDuration)
        self.layout.insertWidget(self.layout.indexOf(self.removeBtn), self.addDurationBtn)

        self.buttons = QDialogButtonBox(QDialogButtonBox.Save | QDialogButtonBox.Cancel)
        self.buttons.accepted.connect(self.onSave)
        self.buttons.rejected.connect(self.reject)
        self.layout.addWidget(self.buttons)

        if self.dropdown_map:
            self.onDurationChanged(0)
        else:
            QMessageBox.information(self, "No durations", "This vendor entry has no base durations to modify!")
            self.buttons.button(QDialogButtonBox.Save).setEnabled(False)
            self.removeBtn.setEnabled(False)
            self.addDurationBtn.setEnabled(False)

    def rebuildDropdown(self):
        self.durationDropdown.clear()
        self.dropdown_map.clear()
        for base_key in self.BASE_KEYS:
            item_id = getFieldValue(self.vendor_entry, base_key)
            if not item_id:
                continue
            item_entry_meta = self.item_lookup.get(item_id)
            item_entry = item_entry_meta[0] if item_entry_meta and isinstance(item_entry_meta, tuple) else item_entry_meta
            label = getFieldValue(item_entry, "ii_name_time") if item_entry else "(Unknown)"
            self.dropdown_map.append((base_key, item_id))
            self.durationDropdown.addItem(f"{base_key[-2:]} — {label}", base_key)

    def onDurationChanged(self, idx):
        if idx < 0 or idx >= len(self.dropdown_map):
            return
        base_key, item_id = self.dropdown_map[idx]

        self.input_item_id.setText(str(item_id))
        self.input_item_id.setReadOnly(idx == 0)  

        for suffix, lineedit in [("_a", self.input_variant_a), ("_b", self.input_variant_b),
                                ("_c", self.input_variant_c), ("_d", self.input_variant_d)]:
            var_id = getFieldValue(self.vendor_entry, f"{base_key}{suffix}") or 0
            lineedit.setText(str(var_id))

        self.removeBtn.setEnabled(idx != 0) 
        item_meta = self.item_lookup.get(item_id)
        item_entry = item_meta[0] if item_meta and isinstance(item_meta, tuple) else item_meta
        if not item_entry:
            self.input_cash.clear()
            self.input_coupon.clear()
            self.input_point.clear()
            return

        self.input_cash.setText(str(getFieldValue(item_entry, "ii_buy_cash", 0)))
        self.input_coupon.setText(str(getFieldValue(item_entry, "ii_buy_coupon", 0)))
        self.input_point.setText(str(getFieldValue(item_entry, "ii_buy_point", 0)))

        if not self.input_item_id.isReadOnly():
            try:
                new_item_id = int(self.input_item_id.text().strip() or 0)
                self._setVendorField(base_key, new_item_id)
            except ValueError:
                QMessageBox.warning(self, "Input error", "Item ID must be an integer.")
                return

        for suffix, lineedit in [("_a", self.input_variant_a), ("_b", self.input_variant_b),
                                ("_c", self.input_variant_c), ("_d", self.input_variant_d)]:
            try:
                new_var_id = int(lineedit.text().strip() or 0)
                self._setVendorField(f"{base_key}{suffix}", new_var_id)
            except ValueError:
                QMessageBox.warning(self, "Input error", f"Variant ID {suffix} must be an integer.")
                return

    def removeCurrentDuration(self):
        idx = self.durationDropdown.currentIndex()
        if idx <= 0:
            return
        base_key, _ = self.dropdown_map[idx]
        n = int(base_key[-2:]) - 1

        for suf in self.VARIANT_SUFFIXES:
            self._setVendorField(f"{self.BASE_KEYS[n]}{suf}", 0)
        for i in range(n, 3):
            src = self.BASE_KEYS[i + 1]
            dst = self.BASE_KEYS[i]
            src_val = getFieldValue(self.vendor_entry, src) or 0
            self._setVendorField(dst, src_val)
            for suf in ["_a", "_b"]:
                srcv = getFieldValue(self.vendor_entry, src + suf) or 0
                self._setVendorField(dst + suf, srcv)
        for suf in self.VARIANT_SUFFIXES:
            self._setVendorField(f"{self.BASE_KEYS[3]}{suf}", 0)

        try:
            for key in [f"{self.BASE_KEYS[n]}{suf}" for suf in self.VARIANT_SUFFIXES]:
                self.cgdManager.cdbs[self.vendor_cdb_name].updateValue(self.vendor_entry_index, key, getFieldValue(self.vendor_entry, key))
            self.cgdManager.saveCdbOutputs(self.cgdManager.cdbs[self.vendor_cdb_name])
        except Exception as e:
            QMessageBox.critical(self, "Save Error", f"Failed to save CDB file:\n{e}")

        self.vendor_entry = self.cgdManager.cdbs[self.vendor_cdb_name].entries[self.vendor_entry_index]
        self.rebuildDropdown()
        self.onDurationChanged(0)
        showToast(self, "Duration removed successfully")

    def onSave(self):
        try:
            for idx, (base_key, item_id) in enumerate(self.dropdown_map):
                item_meta = self.item_lookup.get(item_id)
                if not item_meta:
                    continue
                item_entry, cdbFileName, entryNumber = item_meta[0], item_meta[1], item_meta[2]

                if idx == self.durationDropdown.currentIndex():
                    try:
                        new_cash = int(self.input_cash.text().strip() or 0)
                        new_coupon = int(self.input_coupon.text().strip() or 0)
                        new_point = int(self.input_point.text().strip() or 0)
                    except ValueError:
                        QMessageBox.warning(self, "Input error", "Enter integer values for prices")
                        return
                    if not self.input_item_id.isReadOnly():
                        try:
                            new_item_id = int(self.input_item_id.text().strip() or 0)
                            self._setVendorField(base_key, new_item_id)
                        except ValueError:
                            QMessageBox.warning(self, "Input error", "Item ID must be an integer")
                            return
                    variant_ids = {}
                    for suffix, lineedit in [("_a", self.input_variant_a), ("_b", self.input_variant_b),
                                            ("_c", self.input_variant_c), ("_d", self.input_variant_d)]:
                        try:
                            variant_ids[suffix] = int(lineedit.text().strip() or 0)
                            self._setVendorField(f"{base_key}{suffix}", variant_ids[suffix])
                        except ValueError:
                            QMessageBox.warning(self, "Input error", f"Variant ID {suffix} must be an integer")
                            return
                else:
                    new_cash = getFieldValue(item_entry, "ii_buy_cash", 0)
                    new_coupon = getFieldValue(item_entry, "ii_buy_coupon", 0)
                    new_point = getFieldValue(item_entry, "ii_buy_point", 0)
                    new_item_id = getFieldValue(self.vendor_entry, base_key)
                    variant_ids = {suf: getFieldValue(self.vendor_entry, f"{base_key}{suf}") or 0 for suf in ["_a","_b","_c","_d"]}

                res_cash = self.cgdManager.updateCdbEntryFast(cdbFileName, entryNumber, "ii_buy_cash", new_cash)
                res_coupon = self.cgdManager.updateCdbEntryFast(cdbFileName, entryNumber, "ii_buy_coupon", new_coupon)
                res_point = self.cgdManager.updateCdbEntryFast(cdbFileName, entryNumber, "ii_buy_point", new_point)
                for res in (res_cash, res_coupon, res_point):
                    if not res["success"]:
                        QMessageBox.critical(self, "Save Error", f"Failed to save CDB file:\n{res['error']}")
                        return

                for suffix, var_id in variant_ids.items():
                    if var_id and var_id != 0:
                        var_meta = self.item_lookup.get(var_id)
                        if var_meta:
                            var_entry, var_cdbFile, var_entryNumber = var_meta[0], var_meta[1], var_meta[2]
                            self.cgdManager.updateCdbEntryFast(var_cdbFile, var_entryNumber, "ii_buy_cash", new_cash)
                            self.cgdManager.updateCdbEntryFast(var_cdbFile, var_entryNumber, "ii_buy_coupon", new_coupon)
                            self.cgdManager.updateCdbEntryFast(var_cdbFile, var_entryNumber, "ii_buy_point", new_point)

            self.cgdManager.saveCdbOutputs(self.cgdManager.cdbs[self.vendor_cdb_name])
            showToast(self, "All changes saved successfully.")
            self.accept()
        except Exception as e:
            QMessageBox.critical(self, "Save Error", f"Failed to save CDB file:\n{e}")


    def _setVendorField(self, key, value):
        for f in self.vendor_entry:
            if f.key == key:
                f.value = value
                return True
        return False

    def addNewDuration(self):
        if not self.dropdown_map:
            QMessageBox.warning(self, "No item", "No durations available for this item.")
            return

        _, reference_item_id = self.dropdown_map[0]
        ref_meta = self.item_lookup.get(reference_item_id)
        reference_entry = ref_meta[0] if ref_meta and isinstance(ref_meta, tuple) else ref_meta
        if not reference_entry:
            QMessageBox.warning(self, "Error", "Base item entry not found.")
            return

        DURATION_ORDER = {
            "1 Day": 1,
            "7 Days": 2, 
            "30 Days": 3,
            "Unlimited": 4
        }

        existing_durations = []
        for _, item_id in self.dropdown_map:
            item_meta = self.item_lookup.get(item_id)
            if item_meta:
                item_entry = item_meta[0] if isinstance(item_meta, tuple) else item_meta
                time_str = getFieldValue(item_entry, "ii_name_time", "")
                
                found_duration = None
                for duration_name in DURATION_ORDER.keys():
                    if duration_name.lower() in time_str.lower():
                        found_duration = duration_name
                        break
                
                if found_duration:
                    existing_durations.append(found_duration)

        base_root = reference_item_id // 10 if reference_item_id < 1_000_000 else reference_item_id // 100

        duration_candidates = {}
        for iid, meta in self.item_lookup.items():
            entry = meta[0] if isinstance(meta, tuple) else meta
            if entry is None:
                continue

            entry_root = iid // 10 if iid < 1_000_000 else iid // 100
            if entry_root != base_root:
                continue

            if iid % 10 != 0:
                continue

            name_option = (getFieldValue(entry, "ii_name_option") or "").lower()
            if "speed" not in name_option:
                continue

            time_str = (getFieldValue(entry, "ii_name_time") or "").lower()
            for duration_name in DURATION_ORDER.keys():
                if duration_name.lower() in time_str:
                    if duration_name not in existing_durations:
                        duration_candidates[duration_name] = iid
                    break

        if not duration_candidates:
            for iid, meta in self.item_lookup.items():
                entry = meta[0] if isinstance(meta, tuple) else meta
                if entry is None:
                    continue
                entry_root = iid // 10 if iid < 1_000_000 else iid // 100
                if entry_root != base_root:
                    continue
                time_str = getFieldValue(entry, "ii_name_time", "")
                for duration_name in DURATION_ORDER.keys():
                    if duration_name.lower() in time_str.lower():
                        if duration_name not in existing_durations and duration_name not in duration_candidates:
                            duration_candidates[duration_name] = iid
                        break
            if not duration_candidates:
                QMessageBox.warning(self, "Duration not found",
                                    "Could not find a suitable item ending with '0' and with 'Speed' in ii_name_option.\n"
                                    "Please check your item database.")
                return

        duration, ok = QInputDialog.getItem(
            self, "Select Duration", "Duration to add:", list(duration_candidates.keys()), 0, False
        )
        if not ok or not duration:
            return

        new_item_id = duration_candidates[duration]

        is_weapon = any(
            "itemweaponsinfo" in cdb.fileName.lower()
            for cdb in self.cgdManager.cdbs.values()
            if new_item_id in [getFieldValue(e, "ii_id") for e in cdb.entries]
        )
        is_part = not is_weapon and "iteminfo" in [cdb.fileName.lower() for cdb in self.cgdManager.cdbs.values()]
        is_accessory = not is_weapon and not is_part

        variants_added = {}

        temp_base_key = None
        for base_key in self.BASE_KEYS:
            if not getFieldValue(self.vendor_entry, base_key):
                self._setVendorField(base_key, new_item_id)
                if is_weapon:
                    self._setVendorField(f"{base_key}_a", 0)
                    self._setVendorField(f"{base_key}_b", 0)
                    variants_added["_a"] = 0
                    variants_added["_b"] = 0
                elif is_part:
                    self._setVendorField(f"{base_key}_a", new_item_id)
                    self._setVendorField(f"{base_key}_b", new_item_id + 1)
                    variants_added["_a"] = new_item_id
                    variants_added["_b"] = new_item_id + 1
                elif is_accessory:
                    name = getFieldValue(reference_entry, "ii_name")
                    if "Head" in name:
                        _a_name, _b_name = "Sniper Bullets", "Shotgun Bullets"
                    elif "Back" in name:
                        _a_name, _b_name = "Gatling Bullets", "Bazooka Bullets"
                    elif "Waist" in name:
                        _a_name, _b_name = "Rifle Bullets", "Grenade Bullets"
                    else:
                        _a_name, _b_name = None, None

                    for var_name, var_key in [(_a_name, f"{base_key}_a"), (_b_name, f"{base_key}_b")]:
                        if var_name is None:
                            self._setVendorField(var_key, 0)
                            variants_added[var_key[-2:]] = 0
                            continue

                        found_id = None
                        for iid, meta in self.item_lookup.items():
                            entry = meta[0] if isinstance(meta, tuple) else meta
                            if entry and getFieldValue(entry, "ii_name") == var_name:
                                found_id = iid
                                break
                        if found_id is None:
                            text, ok = QInputDialog.getInt(self, "Missing Variant", f"Enter item ID for '{var_name}':", 0)
                            found_id = text if ok else 0

                        self._setVendorField(var_key, found_id)
                        variants_added[var_key[-2:]] = found_id

                self._setVendorField(f"{base_key}_c", 0)
                self._setVendorField(f"{base_key}_d", 0)
                variants_added["_c"] = 0
                variants_added["_d"] = 0
                
                temp_base_key = base_key
                break
        else:
            QMessageBox.warning(self, "No slot", "No available slot to add a new duration.")
            return

        all_durations = []
        for base_key in self.BASE_KEYS:
            item_id = getFieldValue(self.vendor_entry, base_key)
            if not item_id:
                continue
                
            item_meta = self.item_lookup.get(item_id)
            if not item_meta:
                continue
                
            item_entry = item_meta[0] if isinstance(item_meta, tuple) else item_meta
            time_str = getFieldValue(item_entry, "ii_name_time", "")
            
            duration_name = None
            for target_duration in DURATION_ORDER.keys():
                if target_duration.lower() in time_str.lower():
                    duration_name = target_duration
                    break
            
            if not duration_name:
                continue
            
            variant_data = {}
            for suffix in self.VARIANT_SUFFIXES:
                variant_key = f"{base_key}{suffix}"
                variant_data[suffix] = getFieldValue(self.vendor_entry, variant_key) or 0
            
            all_durations.append({
                'duration_name': duration_name,
                'item_id': item_id,
                'base_key': base_key,
                'variant_data': variant_data,
                'order': DURATION_ORDER.get(duration_name, 99)
            })

        all_durations.sort(key=lambda x: x['order'])

        for base_key in self.BASE_KEYS:
            for suffix in self.VARIANT_SUFFIXES:
                self._setVendorField(f"{base_key}{suffix}", 0)
            self._setVendorField(base_key, 0)

        for i, duration_data in enumerate(all_durations):
            if i >= len(self.BASE_KEYS):
                break  
                
            base_key = self.BASE_KEYS[i]
            self._setVendorField(base_key, duration_data['item_id'])
            
            for suffix, value in duration_data['variant_data'].items():
                self._setVendorField(f"{base_key}{suffix}", value)

        new_vi_id = getFieldValue(self.vendor_entry, "vi_list_01")
        if new_vi_id:
            current_vi_id = getFieldValue(self.vendor_entry, "vi_id")
            for existing_vi_id, vendor_meta in self.vendor_lookup.items():
                if existing_vi_id == new_vi_id and existing_vi_id != current_vi_id:
                    QMessageBox.critical(
                        self, 
                        "Duplicate Vendor ID", 
                        f"Vendor ID {new_vi_id} already exists in the vendor database!\n"
                        f"Cannot add duration as it would create a duplicate vendor entry."
                    )
                    self.vendor_entry = self.cgdManager.cdbs[self.vendor_cdb_name].entries[self.vendor_entry_index]
                    self.rebuildDropdown()
                    if self.dropdown_map:
                        self.onDurationChanged(0)
                    return
            
            self._setVendorField("vi_id", new_vi_id)

        try:
            for key in [f"{k}{suf}" for k in self.BASE_KEYS for suf in self.VARIANT_SUFFIXES] + ["vi_id"]:
                self.cgdManager.cdbs[self.vendor_cdb_name].updateValue(
                    self.vendor_entry_index, key, getFieldValue(self.vendor_entry, key)
                )
            self.cgdManager.saveCdbOutputs(self.cgdManager.cdbs[self.vendor_cdb_name])
        except Exception as e:
            QMessageBox.critical(self, "Save Error", f"Failed to save CDB file:\n{e}")
            return

        self.vendor_entry = self.cgdManager.cdbs[self.vendor_cdb_name].entries[self.vendor_entry_index]
        self.rebuildDropdown()
        self.onDurationChanged(0)

        print(f"Added Duration '{duration}' with ID: {new_item_id}")
        print(f"Updated vi_id to: {new_vi_id}")
        print("Variants assigned:")
        for k, v in variants_added.items():
            print(f"  {k}: {v}")

        showToast(self, f"Duration '{duration}' added successfully.")

