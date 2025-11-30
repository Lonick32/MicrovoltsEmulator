from PySide6.QtWidgets import (
    QVBoxLayout, QComboBox, QPushButton, QDialog, QFormLayout, QLineEdit,
    QDialogButtonBox, QMessageBox, QLabel, QInputDialog, QHBoxLayout
)
from PySide6.QtCore import Qt
from Utils import getFieldValue, showToast


class AddNewVendorItemDialog(QDialog):
    BASE_KEYS = ["vi_list_01", "vi_list_02", "vi_list_03", "vi_list_04"]
    VARIANT_SUFFIXES = ["", "_a", "_b", "_c", "_d"]

    def __init__(self, parent, shop_manager):
        super().__init__(parent)
        self.shop_manager = shop_manager
        self.cgdManager = shop_manager.cgdManager
        self.item_lookup = shop_manager.item_lookup
        self.icon_lookup = shop_manager.icon_lookup
        self.item_icon_path = shop_manager.item_icon_path
        self.vendor_cdb_name = None

        self.setWindowTitle("Add New Vendor Item")
        self.resize(640, 420)

        self.layout = QVBoxLayout(self)
        form = QFormLayout()

        self.input_base_id = QLineEdit()
        form.addRow("Base Item ID (1 Day for RT/MP, Unlimited for Coupon, usually ends with 0):", self.input_base_id)

        self.currencyDropdown = QComboBox()
        self.currencyDropdown.addItems(["RockTokens", "MicroPoints", "Coupons"])
        self.currencyDropdown.currentIndexChanged.connect(self._on_currency_changed)
        form.addRow("Currency:", self.currencyDropdown)

        self.input_desc = QLineEdit()
        form.addRow("Description (optional):", self.input_desc)

        prices_layout = QVBoxLayout()
        self.price_inputs = {} 
        prices_layout.addWidget(QLabel("Prices (enter integers):"))

        for name in ("1 Day", "7 Days", "30 Days", "Unlimited"):
            le = QLineEdit()
            le.setPlaceholderText(name)
            self.price_inputs[name] = le
            prices_layout.addWidget(QLabel(name))
            prices_layout.addWidget(le)

        self.layout.addLayout(form)
        self.layout.addLayout(prices_layout)

        btns = QHBoxLayout()
        self.addBtn = QPushButton("Create Vendor Entry")
        self.addBtn.clicked.connect(self.onCreate)
        self.cancelBtn = QPushButton("Cancel")
        self.cancelBtn.clicked.connect(self.reject)
        btns.addWidget(self.addBtn)
        btns.addWidget(self.cancelBtn)
        self.layout.addLayout(btns)

        for name, cdb in self.cgdManager.cdbs.items():
            if "vendorinfo" in name.lower():
                self.vendor_cdb_name = name
                break

        if not self.vendor_cdb_name:
            QMessageBox.critical(self, "Error", "vendorinfo CDB not found in cgdManager")
            self.addBtn.setEnabled(False)

        self._on_currency_changed(self.currencyDropdown.currentIndex())

    def _on_currency_changed(self, idx):
        pay_type = self.currencyDropdown.currentText()
        print(f"[ADD] Currency selected: {pay_type}")
        if pay_type == "RockTokens":
            for k in ("1 Day", "7 Days", "30 Days"):
                self.price_inputs[k].setEnabled(True)
            self.price_inputs["Unlimited"].setEnabled(False)
            self.price_inputs["Unlimited"].clear()
        elif pay_type == "MicroPoints":
            for k in self.price_inputs:
                self.price_inputs[k].setEnabled(True)
        elif pay_type == "Coupons":
            for k in ("1 Day", "7 Days", "30 Days"):
                self.price_inputs[k].setEnabled(False)
                self.price_inputs[k].clear()
            self.price_inputs["Unlimited"].setEnabled(True)
        print(f"[ADD] Price fields updated for {pay_type}")

    def onCreate(self):
        try:
            base_id = int(self.input_base_id.text().strip())
        except Exception:
            QMessageBox.warning(self, "Input error", "Enter a valid integer for Base Item ID")
            return

        currency = self.currencyDropdown.currentText()
        desc = self.input_desc.text().strip() or "Default shop item description"

        if currency == "Coupons":
            if not self._item_exists(base_id):
                QMessageBox.critical(self, "Error", f"Item ID {base_id} not found in item databases")
                print(f"[ERROR] Item {base_id} not found for Coupons")
                return
        else:
            if not self._item_exists(base_id):
                QMessageBox.critical(self, "Error", f"Item ID {base_id} not found in item databases")
                return

        price_values = {}
        try:
            if currency == "MicroPoints":
                for k in ("1 Day", "7 Days", "30 Days", "Unlimited"):
                    txt = self.price_inputs[k].text().strip() or "0"
                    price_values[k] = int(txt)
            elif currency == "RockTokens":
                for k in ("1 Day", "7 Days", "30 Days"):
                    txt = self.price_inputs[k].text().strip() or "0"
                    price_values[k] = int(txt)
            elif currency == "Coupons":
                txt = self.price_inputs["Unlimited"].text().strip() or "0"
                price_values["Unlimited"] = int(txt)
        except ValueError:
            QMessageBox.warning(self, "Input error", "Enter integer values for prices")
            return

        vendor_entries = self.cgdManager.cdbs[self.vendor_cdb_name].entries
        existing_arrays = [getFieldValue(e, "vi_array_none") or 0 for e in vendor_entries]
        new_array = (max(existing_arrays) + 1) if existing_arrays else 1
        print(f"[ADD] Computed new array value: {new_array}")

        main = self.shop_manager.currentMainSelection
        sub = self.shop_manager.currentSubSelection
        print(f"[ADD] Current shop selection: main={main}, sub={sub}")
        if not main or not sub:
            QMessageBox.critical(self, "Error", "Select a category first in the Shop Manager")
            return

        parent_name = None
        for i in range(self.shop_manager.leftTree.topLevelItemCount()):
            top_item = self.shop_manager.leftTree.topLevelItem(i)
            for j in range(top_item.childCount()):
                child = top_item.child(j)
                if child.text(0) == sub:
                    parent_name = top_item.text(0)
                    break
            if parent_name:
                break
        cat_id = (self.shop_manager.CATEGORY_MAP["Weapons"].get(sub)
                  if main == "Weapons"
                  else self.shop_manager.CATEGORY_MAP.get(parent_name, {}).get(sub))
        if cat_id is None:
            QMessageBox.critical(self, "Error", "Unable to infer category id for selection")
            return

        print(f"[ADD] Resolved category id: {cat_id} (parent_name={parent_name})")
        template = vendor_entries[0] if vendor_entries else None
        new_entry = None
        if template is not None:
            from copy import deepcopy
            new_entry = deepcopy(template)
            for f in new_entry:
                if f.key in ["vi_desc"]:
                    f.value = desc
                elif f.key == "vi_isgift":
                    f.value = True
                else:
                    f.value = 0
        else:
            QMessageBox.critical(self, "Error", "No vendor entry template available to clone")
            return

        self._set_field_in_entry(new_entry, "vi_type", 0)
        self._set_field_in_entry(new_entry, "vi_array_none", new_array)
        self._set_field_in_entry(new_entry, "vi_array_new", new_array)
        self._set_field_in_entry(new_entry, "vi_array_hit", new_array)
        self._set_field_in_entry(new_entry, "vi_list_type", 0)
        self._set_field_in_entry(new_entry, "vi_category", cat_id)
        self._set_field_in_entry(new_entry, "vi_desc", desc)
        self._set_field_in_entry(new_entry, "vi_isgift", True)

        is_weapon = self._is_weapon(base_id)
        print(f"[ADD] base_id {base_id} is_weapon={is_weapon}")

        if currency == "Coupons":
            unlimited_id = base_id
            print(f"[ADD] Coupon: using unlimited item id {unlimited_id}")
            self._set_field_in_entry(new_entry, "vi_list_01", unlimited_id)
            self._set_field_in_entry(new_entry, "vi_list_01_a", 0)
            self._set_field_in_entry(new_entry, "vi_list_01_b", 0)
            self._set_field_in_entry(new_entry, "vi_list_01_c", 0)
            self._set_field_in_entry(new_entry, "vi_list_01_d", 0)
            for i in range(2, 5):
                self._set_field_in_entry(new_entry, f"vi_list_0{i}", 0)
                self._set_field_in_entry(new_entry, f"vi_list_0{i}_a", 0)
                self._set_field_in_entry(new_entry, f"vi_list_0{i}_b", 0)
                self._set_field_in_entry(new_entry, f"vi_list_0{i}_c", 0)
                self._set_field_in_entry(new_entry, f"vi_list_0{i}_d", 0)

            print(f"[ADD] Updating coupon price for item {unlimited_id} -> {price_values.get('Unlimited', 0)}")
            try:
                self._update_item_price_for_id(unlimited_id, cash=0, coupon=price_values.get("Unlimited", 0), point=0)
            except Exception as e:
                QMessageBox.critical(self, "Save Error", f"Failed to update item price: {e}")
                print(f"[ERROR] Failed to update coupon price for {unlimited_id}: {e}")
                return

            self._set_field_in_entry(new_entry, "vi_id", unlimited_id)

        elif currency == "RockTokens":
            one_day = base_id
            durations = self._find_family_durations(one_day)
            print(f"[ADD] Found durations for base {one_day}: {durations}")
            seven = durations.get("7 Days")
            thirty = durations.get("30 Days")

            if seven is None or thirty is None:
                QMessageBox.critical(self, "Error", "Could not find required durations (7 Days / 30 Days) for this base item")
                print(f"[ERROR] Missing durations for RT: 7d={seven}, 30d={thirty}")
                return

            self._set_field_in_entry(new_entry, "vi_list_01", one_day)
            self._set_field_in_entry(new_entry, "vi_list_02", seven)
            self._set_field_in_entry(new_entry, "vi_list_03", thirty)
            self._set_field_in_entry(new_entry, "vi_list_04", 0)

            self._assign_variants_for_new_entry(new_entry, one_day, is_weapon)
            self._propagate_prices_to_variants(new_entry, currency)

            print(f"[ADD] Updating RT prices: 1d={price_values.get('1 Day', 0)}, 7d={price_values.get('7 Days', 0)}, 30d={price_values.get('30 Days', 0)}")
            try:
                self._update_item_price_for_id(one_day, cash=price_values.get("1 Day", 0), coupon=0, point=0)
                self._update_item_price_for_id(seven, cash=price_values.get("7 Days", 0), coupon=0, point=0)
                self._update_item_price_for_id(thirty, cash=price_values.get("30 Days", 0), coupon=0, point=0)
            except Exception as e:
                QMessageBox.critical(self, "Save Error", f"Failed to update item price: {e}")
                print(f"[ERROR] Failed to update RT prices: {e}")
                return

            self._set_field_in_entry(new_entry, "vi_id", one_day)

        elif currency == "MicroPoints":
            one_day = base_id
            durations = self._find_family_durations(one_day)
            seven = durations.get("7 Days")
            thirty = durations.get("30 Days")
            unlimited = durations.get("Unlimited")

            print(f"[ADD] Found durations for base {one_day}: {durations}")

            if not (seven and thirty and unlimited):
                QMessageBox.critical(self, "Error", "Could not find required durations (7/30/Unlimited) for this base item")
                print(f"[ERROR] Missing durations for MP: 7d={seven},30d={thirty},unlimited={unlimited}")
                return

            self._set_field_in_entry(new_entry, "vi_list_01", one_day)
            self._set_field_in_entry(new_entry, "vi_list_02", seven)
            self._set_field_in_entry(new_entry, "vi_list_03", thirty)
            self._set_field_in_entry(new_entry, "vi_list_04", unlimited)

            self._assign_variants_for_new_entry(new_entry, one_day, is_weapon)
            self._propagate_prices_to_variants(new_entry, currency)

            print(f"[ADD] Updating MP prices: 1d={price_values.get('1 Day', 0)}, 7d={price_values.get('7 Days', 0)}, 30d={price_values.get('30 Days', 0)}, unlimited={price_values.get('Unlimited', 0)}")
            try:
                self._update_item_price_for_id(one_day, point=price_values.get("1 Day", 0), cash=0, coupon=0)
                self._update_item_price_for_id(seven, point=price_values.get("7 Days", 0), cash=0, coupon=0)
                self._update_item_price_for_id(thirty, point=price_values.get("30 Days", 0), cash=0, coupon=0)
                self._update_item_price_for_id(unlimited, point=price_values.get("Unlimited", 0), cash=0, coupon=0)
            except Exception as e:
                QMessageBox.critical(self, "Save Error", f"Failed to update item price: {e}")
                print(f"[ERROR] Failed to update MP prices: {e}")
                return

            self._set_field_in_entry(new_entry, "vi_id", one_day)

        new_vi_id = getFieldValue(new_entry, "vi_id")
        for existing in vendor_entries:
            if getFieldValue(existing, "vi_id") == new_vi_id:
                QMessageBox.critical(self, "Duplicate Vendor ID", f"Vendor ID {new_vi_id} already exists")
                print(f"[ERROR] Duplicate vi_id detected: {new_vi_id}")
                return
            if getFieldValue(existing, "vi_array_none") == new_array:
                QMessageBox.critical(self, "Duplicate Array", f"Array value {new_array} already exists")
                print(f"[ERROR] Duplicate array value detected: {new_array}")
                return

        try:
            self.cgdManager.cdbs[self.vendor_cdb_name].entries.append(new_entry)
            keys_to_update = []
            for f in new_entry:
                keys_to_update.append((f.key, f.value))
            idx = len(self.cgdManager.cdbs[self.vendor_cdb_name].entries) - 1
            for key, val in keys_to_update:
                self.cgdManager.cdbs[self.vendor_cdb_name].updateValue(idx, key, val)
            self.cgdManager.saveCdbOutputs(self.cgdManager.cdbs[self.vendor_cdb_name])
            print(f"[ADD] Saved new vendor entry at index {idx} with vi_id={getFieldValue(new_entry,'vi_id')}")

        except Exception as e:
            QMessageBox.critical(self, "Save Error", f"Failed to save new vendor entry: {e}")
            print(f"[ERROR] Failed to save new vendor entry: {e}")
            return

        self.shop_manager.buildLookups()
        self.shop_manager.refreshRelevantItems()

        showToast(self, "New vendor entry created")
        self.accept()

    def _item_exists(self, iid):
        exists = iid in self.item_lookup
        print(f"[ADD] _item_exists({iid}) -> {exists}")
        return exists

    def _is_weapon(self, iid):
        for cdb in self.cgdManager.cdbs.values():
            if "itemweaponsinfo" in cdb.fileName.lower():
                for e in cdb.entries:
                    if getFieldValue(e, "ii_id") == iid:
                        print(f"[ADD] _is_weapon({iid}) -> True (found in {cdb.fileName})")
                        return True
        print(f"[ADD] _is_weapon({iid}) -> False")
        return False

    def _find_family_durations(self, reference_item_id):
        durations = {}
        duration_names = ["1 Day", "7 Days", "30 Days", "Unlimited"]

        base_root = reference_item_id // 10 if reference_item_id < 1_000_000 else reference_item_id // 100
        for iid, meta in self.item_lookup.items():
            entry = meta[0] if isinstance(meta, tuple) else meta
            if entry is None:
                continue

            entry_root = iid // 10 if iid < 1_000_000 else iid // 100
            if entry_root != base_root:
                continue

            if iid % 10 != 0:
                continue

            name_opt = (getFieldValue(entry, "ii_name_option") or "").lower()
            if "speed" not in name_opt:
                continue

            time_str = (getFieldValue(entry, "ii_name_time") or "").lower()

            if "1 day" in time_str:
                durations["1 Day"] = iid
            elif "7 day" in time_str:
                durations["7 Days"] = iid
            elif "30 day" in time_str:
                durations["30 Days"] = iid
            elif "unlimited" in time_str:
                durations["Unlimited"] = iid

        if len(durations) == 4:
            print(f"[ADD] _find_family_durations({reference_item_id}) -> {durations} (primary logic)")
            return durations

        print(f"[WARN] Primary duration logic incomplete for {reference_item_id}, falling back…")
        for iid, meta in self.item_lookup.items():
            if iid in durations.values(): 
                continue

            entry = meta[0] if isinstance(meta, tuple) else meta
            if entry is None:
                continue

            entry_root = iid // 10 if iid < 1_000_000 else iid // 100
            if entry_root != base_root:
                continue

            time = (getFieldValue(entry, "ii_name_time") or "").lower()

            if "1 day" in time and "1 Day" not in durations:
                durations["1 Day"] = iid
            elif "7 day" in time and "7 Days" not in durations:
                durations["7 Days"] = iid
            elif "30 day" in time and "30 Days" not in durations:
                durations["30 Days"] = iid
            elif "unlimited" in time and "Unlimited" not in durations:
                durations["Unlimited"] = iid

        missing = [d for d in duration_names if d not in durations]
        if missing:
            QMessageBox.critical(
                self, "Duration Missing",
                f"Could not find required durations ({', '.join(missing)}) for item {reference_item_id}"
            )
            raise ValueError(f"Missing durations: {missing}")

        print(f"[ADD] _find_family_durations({reference_item_id}) -> {durations} (fallback)")
        return durations


    def _assign_variants_for_new_entry(self, vendor_entry, base_item_id, is_weapon):
        is_part = not is_weapon and any("iteminfo" in cdb.fileName.lower() for cdb in self.cgdManager.cdbs.values())
        for base_key in self.BASE_KEYS:
            item_id = getFieldValue(vendor_entry, base_key)
            if not item_id:
                continue
            print(f"[ADD] Assigning variants for base_key={base_key}, item_id={item_id}, is_weapon={is_weapon}, is_part={is_part}")
            if is_weapon:
                for suf in ["_a", "_b", "_c", "_d"]:
                    self._set_field_in_entry(vendor_entry, f"{base_key}{suf}", 0)
            elif is_part:
                self._set_field_in_entry(vendor_entry, f"{base_key}_a", item_id)
                self._set_field_in_entry(vendor_entry, f"{base_key}_b", item_id + 1)
                self._set_field_in_entry(vendor_entry, f"{base_key}_c", 0)
                self._set_field_in_entry(vendor_entry, f"{base_key}_d", 0)
            else:
                item_meta = self.item_lookup.get(item_id)
                entry = item_meta[0] if item_meta and isinstance(item_meta, tuple) else item_meta
                name = getFieldValue(entry, "ii_name") if entry else ""

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
                        self._set_field_in_entry(vendor_entry, var_key, 0)
                        continue
                    found_id = None
                    for iid, meta in self.item_lookup.items():
                        entry = meta[0] if isinstance(meta, tuple) else meta
                        if entry and getFieldValue(entry, "ii_name") == var_name:
                            found_id = iid
                            break
                    if found_id is None:
                        val, ok = QInputDialog.getInt(self, "Missing Variant", f"Enter item ID for '{var_name}':", 0)
                        found_id = val if ok else 0
                    self._set_field_in_entry(vendor_entry, var_key, found_id)
                self._set_field_in_entry(vendor_entry, f"{base_key}_c", 0)
                self._set_field_in_entry(vendor_entry, f"{base_key}_d", 0)

    def _update_item_price_for_id(self, iid, cash=None, coupon=None, point=None):
        meta = self.item_lookup.get(iid)
        if not meta:
            print(f"[WARN] _update_item_price_for_id: item {iid} not found in lookup")
            return False
        entry, cdbFileName, entryNumber = meta[0], meta[1], meta[2]
        if cash is not None:
            print(f"[ADD] Updating ii_buy_cash for {iid} -> {cash}")
            res = self.cgdManager.updateCdbEntryFast(cdbFileName, entryNumber, "ii_buy_cash", int(cash))
            if not res.get("success"):
                raise Exception(res.get("error") or "Failed to update cash")
        if coupon is not None:
            print(f"[ADD] Updating ii_buy_coupon for {iid} -> {coupon}")
            res = self.cgdManager.updateCdbEntryFast(cdbFileName, entryNumber, "ii_buy_coupon", int(coupon))
            if not res.get("success"):
                raise Exception(res.get("error") or "Failed to update coupon")
        if point is not None:
            print(f"[ADD] Updating ii_buy_point for {iid} -> {point}")
            res = self.cgdManager.updateCdbEntryFast(cdbFileName, entryNumber, "ii_buy_point", int(point))
            if not res.get("success"):
                raise Exception(res.get("error") or "Failed to update point")
        print(f"[ADD] Price update completed for {iid}")
        return True

    def _set_field_in_entry(self, vendor_entry, key, value):
        for f in vendor_entry:
            if f.key == key:
                f.value = value
                return True
        try:
            class FakeField:
                def __init__(self, k, v):
                    self.key = k
                    self.value = v
            vendor_entry.append(FakeField(key, value))
            return True
        except Exception:
            return False

    def _propagate_prices_to_variants(self, vendor_entry, currency):
        for base_key in self.BASE_KEYS:
            base_id = getFieldValue(vendor_entry, base_key)
            if not base_id:
                continue

            if currency == "RockTokens":
                cash_price = self._get_price_for_item(base_id, "cash")
                coupon_price = 0
                point_price = 0
            elif currency == "MicroPoints":
                cash_price = 0
                coupon_price = 0
                point_price = self._get_price_for_item(base_id, "point")
            elif currency == "Coupons":
                cash_price = 0
                coupon_price = self._get_price_for_item(base_id, "coupon")
                point_price = 0
            else:
                cash_price = coupon_price = point_price = 0

            self._update_item_price_for_id(base_id, cash=cash_price, coupon=coupon_price, point=point_price)
            for suf in ["_a", "_b", "_c", "_d"]:
                var_id = getFieldValue(vendor_entry, f"{base_key}{suf}")
                if var_id:
                    print(f"[ADD] Propagating price to variant {var_id} for {base_key}{suf}")
                    self._update_item_price_for_id(var_id, cash=cash_price, coupon=coupon_price, point=point_price)

            all_ids = [base_id] + [getFieldValue(vendor_entry, f"{base_key}{suf}") or 0 for suf in ["_a","_b","_c","_d"]]
            for iid in all_ids:
                if iid == 0:
                    continue
                if currency != "RockTokens":
                    self._update_item_price_for_id(iid, cash=0)
                if currency != "MicroPoints":
                    self._update_item_price_for_id(iid, point=0)
                if currency != "Coupons":
                    self._update_item_price_for_id(iid, coupon=0)

    def _get_price_for_item(self, iid, price_type):
        durations = self._find_family_durations(iid)
        for name, id_val in durations.items():
            if id_val == iid:
                if price_type == "cash":
                    return int(self.price_inputs.get(name, QLineEdit()).text() or 0)
                elif price_type == "point":
                    return int(self.price_inputs.get(name, QLineEdit()).text() or 0)
                elif price_type == "coupon":
                    return int(self.price_inputs.get("Unlimited", QLineEdit()).text() or 0)
        return 0
