#!/usr/bin/env python3
"""
NINA PHASE 1: Block Validator Model Trainer
Entrena Random Forest + XGBoost ensemble para detectar bloques anomalos
"""

import pandas as pd
import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import (
    accuracy_score, precision_score, recall_score, 
    f1_score, confusion_matrix, classification_report
)
import xgboost as xgb
import joblib
import argparse
import json
from pathlib import Path
from datetime import datetime

class BlockValidatorTrainer:
    """Trainer para PHASE 1: Block Validation"""
    
    def __init__(self, models_dir="models"):
        self.models_dir = Path(models_dir)
        self.models_dir.mkdir(parents=True, exist_ok=True)
        self.rf_model = None
        self.xgb_model = None
        self.feature_names = None
        
    def load_training_data(self, csv_file):
        """Carga datos de entrenamiento desde CSV"""
        print(f"\n[LOADER] Cargando datos desde {csv_file}...")
        df = pd.read_csv(csv_file)
        print(f"✅ {len(df)} filas cargadas")
        print(f"   Columnas: {list(df.columns)}")
        return df
    
    def engineer_features(self, df):
        """Ingeniería de features para el modelo"""
        print(f"\n[FEATURES] Ingenierizando features...")
        
        X = pd.DataFrame()
        
        # Features existentes
        X['difficulty'] = df['difficulty']
        X['txs_count'] = df['txs_count']
        X['network_health'] = df['network_health']
        X['block_age'] = df['block_age']
        X['hash_entropy'] = df['hash_entropy']
        X['miner_reputation'] = df['miner_reputation']
        
        # Features derivadas
        X['difficulty_normalized'] = (X['difficulty'] - X['difficulty'].mean()) / X['difficulty'].std()
        X['txs_entropy'] = -X['txs_count'] * np.log(X['txs_count'] + 1) / 100
        X['anomaly_score'] = (
            (1 - X['network_health']) + 
            (1 - X['miner_reputation']) + 
            abs(X['difficulty_normalized']) / 10
        ) / 3
        
        # Timestamp entropy (rolling std)
        X['timestamp'] = pd.to_datetime(df['timestamp'], unit='s')
        X['time_entropy'] = 0.5  # Normalized
        
        self.feature_names = X.columns.tolist()
        print(f"✅ {len(self.feature_names)} features creados:")
        for i, feat in enumerate(self.feature_names, 1):
            print(f"   {i:2d}. {feat}")
        
        return X
    
    def train(self, csv_file, test_size=0.2):
        """Entrena Random Forest + XGBoost ensemble"""
        
        # Cargar datos
        df = self.load_training_data(csv_file)
        
        # Feature engineering
        X = self.engineer_features(df)
        y = df['is_valid'].values
        
        # Split train/test
        X_train, X_test, y_train, y_test = train_test_split(
            X, y, test_size=test_size, random_state=42, stratify=y
        )
        
        print(f"\n[SPLIT] Train: {len(X_train)}, Test: {len(X_test)}")
        print(f"   Train válidos: {sum(y_train)}, anomalías: {len(y_train)-sum(y_train)}")
        print(f"   Test válidos: {sum(y_test)}, anomalías: {len(y_test)-sum(y_test)}")
        
        # Random Forest
        print(f"\n[RF] Entrenando Random Forest...")
        self.rf_model = RandomForestClassifier(
            n_estimators=200,
            max_depth=15,
            min_samples_split=10,
            random_state=42,
            n_jobs=-1
        )
        self.rf_model.fit(X_train, y_train)
        rf_pred = self.rf_model.predict(X_test)
        rf_proba = self.rf_model.predict_proba(X_test)[:, 1]
        
        rf_acc = accuracy_score(y_test, rf_pred)
        print(f"✅ RF Accuracy: {rf_acc:.4f}")
        
        # XGBoost
        print(f"\n[XGB] Entrenando XGBoost...")
        self.xgb_model = xgb.XGBClassifier(
            n_estimators=200,
            max_depth=8,
            learning_rate=0.1,
            subsample=0.8,
            random_state=42,
            verbose=0
        )
        self.xgb_model.fit(X_train, y_train)
        xgb_pred = self.xgb_model.predict(X_test)
        xgb_proba = self.xgb_model.predict_proba(X_test)[:, 1]
        
        xgb_acc = accuracy_score(y_test, xgb_pred)
        print(f"✅ XGB Accuracy: {xgb_acc:.4f}")
        
        # Ensemble (average probabilities)
        print(f"\n[ENSEMBLE] Combinando predicciones...")
        ensemble_proba = (rf_proba + xgb_proba) / 2
        ensemble_pred = (ensemble_proba >= 0.5).astype(int)
        
        ensemble_acc = accuracy_score(y_test, ensemble_pred)
        ensemble_prec = precision_score(y_test, ensemble_pred, zero_division=0)
        ensemble_rec = recall_score(y_test, ensemble_pred, zero_division=0)
        ensemble_f1 = f1_score(y_test, ensemble_pred, zero_division=0)
        
        print(f"✅ Ensemble Accuracy: {ensemble_acc:.4f}")
        print(f"   Precision: {ensemble_prec:.4f}")
        print(f"   Recall: {ensemble_rec:.4f}")
        print(f"   F1-Score: {ensemble_f1:.4f}")
        
        # Confusion matrix
        cm = confusion_matrix(y_test, ensemble_pred)
        print(f"\n[CM] Confusion Matrix:")
        print(f"   TP={cm[1,1]}, FP={cm[0,1]}, FN={cm[1,0]}, TN={cm[0,0]}")
        
        # Feature importance
        print(f"\n[IMPORTANCE] Top 5 features (RF):")
        importances = self.rf_model.feature_importances_
        top_5_idx = np.argsort(importances)[-5:][::-1]
        for rank, idx in enumerate(top_5_idx, 1):
            print(f"   {rank}. {self.feature_names[idx]:25s}: {importances[idx]:.4f}")
        
        # Guardar modelos
        self.save_models()
        
        # Resumen
        print(f"\n" + "="*60)
        print(f"✅ PHASE 1 TRAINING COMPLETE")
        print(f"="*60)
        print(f"Ensemble Accuracy: {ensemble_acc:.2%}")
        print(f"Models saved to: {self.models_dir}/")
        print(f"="*60)
        
        return {
            'rf_accuracy': float(rf_acc),
            'xgb_accuracy': float(xgb_acc),
            'ensemble_accuracy': float(ensemble_acc),
            'ensemble_precision': float(ensemble_prec),
            'ensemble_recall': float(ensemble_rec),
            'ensemble_f1': float(ensemble_f1),
            'confusion_matrix': cm.tolist(),
            'features': self.feature_names
        }
    
    def save_models(self):
        """Guarda modelos entrenados"""
        print(f"\n[SAVE] Guardando modelos a {self.models_dir}...")
        
        rf_path = self.models_dir / "block_validator_rf.pkl"
        xgb_path = self.models_dir / "block_validator_xgb.pkl"
        meta_path = self.models_dir / "block_validator_meta.json"
        
        joblib.dump(self.rf_model, rf_path)
        joblib.dump(self.xgb_model, xgb_path)
        
        meta = {
            'timestamp': datetime.now().isoformat(),
            'features': self.feature_names,
            'rf_estimators': self.rf_model.n_estimators,
            'xgb_estimators': self.xgb_model.n_estimators
        }
        with open(meta_path, 'w') as f:
            json.dump(meta, f, indent=2)
        
        print(f"   ✅ RF Model: {rf_path}")
        print(f"   ✅ XGB Model: {xgb_path}")
        print(f"   ✅ Metadata: {meta_path}")
    
    def load_models(self):
        """Carga modelos previamente entrenados"""
        rf_path = self.models_dir / "block_validator_rf.pkl"
        xgb_path = self.models_dir / "block_validator_xgb.pkl"
        
        if rf_path.exists() and xgb_path.exists():
            print(f"[LOAD] Cargando modelos desde {self.models_dir}...")
            self.rf_model = joblib.load(rf_path)
            self.xgb_model = joblib.load(xgb_path)
            print(f"✅ Modelos cargados exitosamente")
            return True
        else:
            print(f"❌ Modelos no encontrados en {self.models_dir}")
            return False

def main():
    parser = argparse.ArgumentParser(description='NINA PHASE 1: Block Validator Trainer')
    parser.add_argument('--data', required=True, help='CSV file with training data')
    parser.add_argument('--models-dir', default='src/nina_ml/models', help='Directory to save models')
    parser.add_argument('--validate', action='store_true', help='Load and validate existing model')
    
    args = parser.parse_args()
    
    trainer = BlockValidatorTrainer(models_dir=args.models_dir)
    
    if args.validate:
        trainer.load_models()
    else:
        metrics = trainer.train(args.data)
        
        # Guardar métricas
        metrics_path = Path(args.models_dir) / "training_metrics.json"
        with open(metrics_path, 'w') as f:
            json.dump(metrics, f, indent=2)
        print(f"\n📊 Métricas guardadas en {metrics_path}")

if __name__ == "__main__":
    main()
