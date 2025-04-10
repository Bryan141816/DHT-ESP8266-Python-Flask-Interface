from flask import Flask, request, render_template, redirect, url_for, flash, session
from flask_sqlalchemy import SQLAlchemy
from sqlalchemy import func
import requests
from datetime import datetime
from flask import jsonify
import hashlib

import pytz

app = Flask(__name__)

app.config['SQLALCHEMY_DATABASE_URI'] = 'mysql+pymysql://root:@localhost/dhtweb'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

db = SQLAlchemy(app)

app.secret_key = 'yoursecretkey' 

local_timezone = pytz.timezone('Asia/Manila') 

class User(db.Model):
    
    __tablename__ = 'users'
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(100), nullable=False, unique=True)
    password = db.Column(db.String(100), nullable=False)

class Readings(db.Model):
    def get_local_time():
        return datetime.now(local_timezone)
    
    __tablename__ = 'readings'
    id = db.Column(db.Integer, primary_key=True)
    temperature = db.Column(db.Numeric(3, 2), nullable=False)
    recorded_at = db.Column(db.DateTime, nullable=False, default=get_local_time)

def hasher(text):
    hash_object = hashlib.sha256(b'example data')
    hex_dig = hash_object.hexdigest()
    return hex_dig

@app.route('/', methods=['GET', 'POST'])
def home():
    if request.method == 'POST':
        input_username = request.form['username']
        input_password = request.form['password']

        user = User.query.filter_by(username=input_username).first()

        if user and user.password == hasher(input_password):
            flash('Login successful!', 'success')
            session['logState'] = True  # Store login state in session
            return redirect(url_for('main'))
        else:
            flash('Invalid username or password.', 'error')

    return render_template('index.html')

@app.route('/signup', methods=['GET','POST'])
def signup():
    if request.method == 'POST':
        input_username = request.form['username']
        input_password = request.form['password']
        input_password2 = request.form['password2']

        user = User.query.filter_by(username=input_username).first()
        if(user):
            flash('User already exist.', 'error')
        else:
            if input_password != input_password2:
                flash('Password should match')
            else:
                new_user = User(username = input_username, password = hasher(input_password))
                db.session.add(new_user)
                db.session.commit()

                return redirect(url_for('home'))

    return render_template('signup.html')


@app.route('/dashboard', methods=['GET'])
def main():
    if session.get('logState', False):  # Check if user is logged in
        return render_template('main.html')
    else:
        return redirect(url_for('home'))


@app.route('/get_data')
def get_data():
    esp_ip = "http://192.168.1.14/data"  # Replace with ESP8266's IP
    try:
        response = requests.get(esp_ip)

        if response.status_code == 200:
            data = response.json()  # ESP returns something like {"temperature": 25.6}
            temp = data.get('temperature')
            new_record = Readings(temperature = float(temp))
            db.session.add(new_record)
            db.session.commit()

            hourly_max_temperatures = (
                db.session.query(
                    func.date_format(Readings.recorded_at, '%h:00').label('hour'),
                    func.max(Readings.temperature).label('max_temperature')
                )
                .group_by(func.date_format(Readings.recorded_at, '%Y-%m-%d %H:00:00'))
                .order_by('hour')
                .all()
            )
            hours = [hour for hour, _ in hourly_max_temperatures]
            temperatures = [float(max_temp) for _, max_temp in hourly_max_temperatures]
            result = {
            "Reading": {
                "Dates": hours,
                "Temperatures": temperatures
            }
}

            return jsonify({"temperature": temp, "Records": result})
        else:
            return jsonify({"error": "Failed to get data from ESP8266"}), 500

    except requests.exceptions.RequestException as e:
        return jsonify({"error": str(e)}), 500

@app.route('/logout')
def logout():
    session.pop('logState', None)  # Remove login state from session
    return redirect(url_for('home'))

if __name__ == '__main__':
    app.run(debug=True)
